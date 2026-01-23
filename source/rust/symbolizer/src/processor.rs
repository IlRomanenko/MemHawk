use anyhow::bail;
use bitcode::{Decode, Encode};
use rustc_hash::{FxHashMap, FxHashSet};
use std::{cell::RefCell, rc::Rc, sync::Arc};
use strum::IntoEnumIterator;
use time::OffsetDateTime;
use tokio::sync::RwLock;
use tokio_util::task::TaskTracker;

use crate::clickhouse::{client::ClickhouseClient, schema::*};
use memhawk_core::{
    graph::{self, AggregatedAllocSummary, Graph, NodeId},
    localizer::{
        self, FrameLocalizer, LocalizedFrameId, LocalizedName, MEMHAWK_ROOT_LOCALIZED_ID,
        OnFrameLocalized, UNKNOWN_LOCALIZED_ID, VecRange,
    },
    proto::schema::*,
    symbolizer::{self, SymbolizedFrame, Symbolizer},
};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Encode, Decode)]
struct TraceId(u32);

struct FramesSubscription {
    updated_names: Vec<(LocalizedName, LocalizedFrameId)>,
}

impl FramesSubscription {
    pub fn new() -> Self {
        Self {
            updated_names: Vec::new(),
        }
    }

    pub fn consume(&mut self, process_id: i32) -> Vec<LocalizedNameRow> {
        let mut localized_names = Vec::new();

        let updated_names = std::mem::take(&mut self.updated_names);
        for (name, id) in updated_names {
            localized_names.push(LocalizedNameRow {
                process_id: process_id,
                label_id: id.into(),
                label: name.name,
                location: name.location,
                library: name.library.to_string(),
                frame_offset: 0,
            });
        }
        localized_names
    }
}

impl OnFrameLocalized for FramesSubscription {
    fn on_new_name(&mut self, name: &LocalizedName, name_id: LocalizedFrameId) {
        self.updated_names.push((name.clone(), name_id));
    }
}

#[derive(Encode, Decode)]
pub struct RestorableState {
    process_id: i32,
    sysroot: Option<String>,

    localizer_state: localizer::RestorableState,
    symbolizer_state: symbolizer::RestorableState,
    graph_state: graph::RestorableState,

    ptr_id_to_addr_map: FxHashMap<u32, u64>,
    trace_id_to_leaf_id: FxHashMap<TraceId, NodeId>,
    raw_data_to_save: FxHashMap<NodeId, AllocSummary>,

    last_processed_timestamp: u64,
}

pub struct Processor {
    process_id: i32,
    sysroot: Option<String>,

    click: ClickhouseClient,

    symbolizer: Arc<RwLock<Symbolizer>>,
    localizer_sub: Rc<RefCell<FramesSubscription>>,
    localizer: FrameLocalizer,

    ptr_id_to_addr_map: FxHashMap<u32, u64>,

    symbolized_graph: Graph, //  graph after symbolyzer for frames

    trace_id_to_leaf_id: FxHashMap<TraceId, NodeId>,

    raw_data_to_save: FxHashMap<NodeId, AllocSummary>,

    last_processed_timestamp: u64,
}

impl Processor {
    pub async fn new(task_tracker: &TaskTracker, process_id: i32, sysroot: Option<String>) -> anyhow::Result<Self> {
        let frames_sub = Rc::new(RefCell::new(FramesSubscription::new()));
        let click = ClickhouseClient::new(task_tracker);
        // clear all leftover state if there is some
        click.clear(process_id).await?;
        let processor = Self {
            process_id,
            sysroot,
            click: click,
            symbolizer: Arc::new(RwLock::new(Symbolizer::new())),
            localizer_sub: frames_sub.clone(),
            localizer: FrameLocalizer::new(frames_sub),
            ptr_id_to_addr_map: FxHashMap::default(),
            symbolized_graph: Graph::new(),
            trace_id_to_leaf_id: FxHashMap::default(),
            raw_data_to_save: FxHashMap::default(),
            last_processed_timestamp: 0,
        };
        Ok(processor)
    }

    pub async fn restore(
        task_tracker: &TaskTracker,
        state: RestorableState,
        sysroot: Option<String>,
    ) -> anyhow::Result<Self> {
        let frames_sub: Rc<RefCell<FramesSubscription>> =
            Rc::new(RefCell::new(FramesSubscription::new()));
        let click = ClickhouseClient::new(task_tracker);
        let symbolizer = Symbolizer::restore(state.symbolizer_state).await;

        if sysroot != state.sysroot {
            bail!(
                "Sysroot differs from saved state, current: {:?}, saved: {:?}",
                sysroot,
                state.sysroot
            );
        }
        let processor = Self {
            process_id: state.process_id,
            sysroot: state.sysroot,
            click: click,
            symbolizer: Arc::new(RwLock::new(symbolizer)),
            localizer_sub: frames_sub.clone(),
            localizer: FrameLocalizer::restore(state.localizer_state, frames_sub),
            ptr_id_to_addr_map: state.ptr_id_to_addr_map,
            symbolized_graph: Graph::restore(state.graph_state),
            trace_id_to_leaf_id: state.trace_id_to_leaf_id,
            raw_data_to_save: state.raw_data_to_save,
            last_processed_timestamp: state.last_processed_timestamp,
        };
        Ok(processor)
    }

    pub async fn save(&self) -> RestorableState {
        RestorableState {
            process_id: self.process_id,
            sysroot: self.sysroot.clone(),
            localizer_state: self.localizer.save(),
            symbolizer_state: self.symbolizer.read().await.save(),
            graph_state: self.symbolized_graph.save(),
            ptr_id_to_addr_map: self.ptr_id_to_addr_map.clone(),
            trace_id_to_leaf_id: self.trace_id_to_leaf_id.clone(),
            raw_data_to_save: self.raw_data_to_save.clone(),
            last_processed_timestamp: self.last_processed_timestamp,
        }
    }

    pub async fn process(&mut self, snapshot: &Snapshot) -> anyhow::Result<()> {
        let timestamp = OffsetDateTime::from_unix_timestamp_nanos(snapshot.timestamp as i128)?;
        log::info!("Processing : {}", timestamp);

        if snapshot.timestamp <= self.last_processed_timestamp {
            log::info!(
                "Skipping snapshot, last processed timestamp: {}",
                self.last_processed_timestamp
            );
            return Ok(());
        }
        self.last_processed_timestamp = self.last_processed_timestamp.max(snapshot.timestamp);

        if !snapshot.loaded_so.is_empty() {
            log::info!("Updating symbols");

            let loaded_so = match &self.sysroot {
                Some(sysroot) => {
                    snapshot.loaded_so.iter().cloned().map(|mut elf_info| {
                        elf_info.filename = sysroot.clone() + "/" + &elf_info.filename;
                        elf_info
                    }).collect::<Vec<_>>()
                }
                None => {
                    snapshot.loaded_so.clone()
                }
            };
            self.symbolizer
                .write()
                .await
                .update_symbols(&loaded_so)
                .await;
        }
        self.process_stacktraces(snapshot).await;
        self.process_allocations(snapshot)?;

        self.process_modifications(timestamp).await?;
        Ok(())
    }

    async fn process_modifications(&mut self, timestamp: OffsetDateTime) -> anyhow::Result<()> {
        self.symbolized_graph.process_postponed_updates();
        let graph_update = self.symbolized_graph.consume_modifications();

        let localized_names = self.localizer_sub.borrow_mut().consume(self.process_id);

        let stacktraces = self.prepare_stacktraces(graph_update.new_leaf_nodes);
        let graph_edges = self.prepare_graph_edges(graph_update.new_nodes);
        let flamegraphs = self.prepare_flamegraphs(timestamp);
        let timeseries = self.prepare_timeseries(timestamp, graph_update.modified_frames);

        let raw_data_to_save = std::mem::take(&mut self.raw_data_to_save);
        let raw_data = self.prepare_raw_data(raw_data_to_save, timestamp);

        let update = UpdateData {
            raw_data,
            flamegraphs,
            timeseries,
            localized_names,
            stacktraces,
            graph_edges,
        };
        log::info!("Update: {:?}", &update);

        self.click.send(update).await?;

        Ok(())
    }

    fn prepare_raw_data(
        &self,
        raw_data: FxHashMap<NodeId, AllocSummary>,
        timestamp: OffsetDateTime,
    ) -> Vec<RawDataRow> {
        let mut result = Vec::new();
        for (node_id, summary) in raw_data.into_iter() {
            result.push(RawDataRow {
                process_id: self.process_id,
                timestamp,
                leaf_node_id: node_id.into(),
                active_size: summary.size,
                active_count: summary.active,
                overhead: summary.overhead,
                total_size: summary.total_bytes,
                total_count: summary.total_count,
            })
        }

        result
    }

    fn prepare_stacktraces(&self, new_leaf_nodes: FxHashSet<NodeId>) -> Vec<StacktraceRow> {
        let mut result = Vec::new();
        let mut path_nodes = Vec::new();
        for node_id in new_leaf_nodes {
            self.symbolized_graph.backtrace(node_id, &mut path_nodes);
            let path = path_nodes
                .iter()
                .copied()
                .map(|x| self.symbolized_graph.inspect(x).key.into())
                .collect::<Vec<_>>();
            result.push(StacktraceRow {
                process_id: self.process_id,
                leaf_node_id: node_id.into(),
                path: path,
            })
        }
        result
    }

    fn prepare_graph_edges(&self, new_nodes: FxHashSet<NodeId>) -> Vec<GraphEdgeRow> {
        let mut result = Vec::new();

        for node_id in new_nodes {
            let node = self.symbolized_graph.inspect(node_id);
            result.push(GraphEdgeRow {
                process_id: self.process_id,
                label_id: node.key.into(),
                node_id: node_id.into(),
                parent_node_id: node.parent.into(),
            })
        }

        result
    }

    fn prepare_flamegraphs(&self, timestamp: OffsetDateTime) -> Vec<FlamegraphRow> {
        let mut result = Vec::new();

        for value_type in memhawk_core::proto::schema::ValueType::iter() {
            let nodes = self.symbolized_graph.get_top(1000, value_type);
            for (order_id, element) in nodes.into_iter().enumerate() {
                let node = self.symbolized_graph.inspect(element.node_id);
                result.push(FlamegraphRow {
                    process_id: self.process_id,
                    timestamp,
                    order_id: order_id as u32,
                    label_id: node.key.into(),
                    level: node.level.into(),
                    value_type: value_type.into(),
                    self_value: element.self_value,
                    total_value: element.total_value,
                })
            }
        }
        result
    }

    fn prepare_timeseries(
        &self,
        timestamp: OffsetDateTime,
        modified_frames: FxHashMap<LocalizedFrameId, AggregatedAllocSummary>,
    ) -> Vec<TimeseriesRow> {
        let mut result = Vec::new();

        for (frame_id, summary) in modified_frames.into_iter() {
            for value_type in memhawk_core::proto::schema::ValueType::iter() {
                result.push(TimeseriesRow {
                    process_id: self.process_id,
                    timestamp,
                    label_id: frame_id.into(),
                    value_type: value_type.into(),
                    self_value: summary
                        .self_value
                        .as_ref()
                        .map_or(0, |x| value_selector(&x, value_type)),
                    total_value: value_selector(&summary.total_value, value_type),
                })
            }
        }

        result
    }

    fn process_allocations(&mut self, snapshot: &Snapshot) -> anyhow::Result<()> {
        for summary in snapshot.changed.iter() {
            match self.process_alloc_summary(summary) {
                Ok(rows) => rows,
                Err(err) => {
                    log::error!("Failed to process AllocSummary: {:?}", err);
                    continue;
                }
            };
        }
        Ok(())
    }

    pub async fn stats(&self) {
        log::info!(
            "Processor::trace_id_to_leaf_id.len(): {}",
            self.trace_id_to_leaf_id.len()
        );
        log::info!(
            "Processor::ptr_id_to_addr_map.len(): {}",
            self.ptr_id_to_addr_map.len()
        );
        self.localizer.stats();
        self.symbolized_graph.stats();
    }

    fn process_alloc_summary(&mut self, summary: &TracedAllocSummary) -> anyhow::Result<()> {
        let actual = match summary.actual {
            Some(actual) => actual,
            None => bail!(
                "TracedAllocSummary without actual summary, traceId: {}",
                summary.trace_id
            ),
        };

        let leaf_id = match self.trace_id_to_leaf_id.get(&TraceId(summary.trace_id)) {
            Some(node_id) => *node_id,
            None => {
                log::warn!(
                    "Encountered allocation with unknown trace_id: {}",
                    summary.trace_id
                );
                self.symbolized_graph
                    .construct_path(&vec![UNKNOWN_LOCALIZED_ID])
            }
        };
        self.raw_data_to_save
            .entry(leaf_id)
            .and_modify(|x| *x += actual)
            .or_insert(actual);
        self.symbolized_graph.postpone_update(leaf_id, actual);
        Ok(())
    }

    async fn symbolize_new_addrs(&mut self, snapshot: &Snapshot) {
        // todo: Rewrite in more idiomatic style
        let mut tasks = Vec::new();
        for elem in snapshot.ptr_ids.iter() {
            let symbolizer = Arc::clone(&self.symbolizer);
            let addr = elem.ptr_addr;
            tasks.push(tokio::spawn(async move {
                let read_lock = symbolizer.read().await;
                (addr, read_lock.lookup_symbol(addr).await)
            }));
        }
        for task in tasks {
            let (addr, frame_res) = task.await.unwrap();
            let frame = match frame_res {
                Ok(frame) => frame,
                Err(err) => {
                    log::info!("Failed to find frame for addr: {addr:x}, err: {err}");
                    SymbolizedFrame {
                        symbol_name: "unknown".to_owned(),
                        library_name: Arc::new("unknown".to_owned()),
                        inlined: Vec::new(),
                        offset: 0,
                    }
                }
            };
            // don't use returned range, because it's cached inside localizer
            let _ = self.localizer.process_symbolized_frame(addr, frame);
        }
    }

    async fn process_stacktraces(&mut self, snapshot: &Snapshot) {
        for pair in snapshot.ptr_ids.iter() {
            self.ptr_id_to_addr_map.insert(pair.ptr_id, pair.ptr_addr);
        }

        self.symbolize_new_addrs(snapshot).await;

        let mut localized_trace = Vec::new();
        let mut addrs = Vec::new();

        for stacktrace in snapshot.stacktraces.iter() {
            localized_trace.clear();
            addrs.clear();

            addrs.extend(
                stacktrace
                    .ptr_id
                    .iter()
                    .filter_map(|x| self.ptr_id_to_addr_map.get(x).copied())
                    .rev(),
            );
            // Check is it a memhawk trace
            if stacktrace.trace_id >= 2_i32.saturating_pow(31) as u32 {
                localized_trace.push(MEMHAWK_ROOT_LOCALIZED_ID);
            }

            for addr in addrs.iter() {
                let range = self.get_localized_range(*addr).await;
                self.localizer
                    .append_localized_range(&range, &mut localized_trace);
            }

            let leaf_node = self.symbolized_graph.construct_path(&localized_trace);
            self.trace_id_to_leaf_id
                .insert(TraceId(stacktrace.trace_id), leaf_node);
        }
    }

    async fn get_localized_range(&mut self, addr: u64) -> VecRange {
        if let Some(range) = self.localizer.get_frame_range(addr) {
            return *range;
        }
        log::warn!("got addr, with unknown symbol, perform additional lookup");
        let frame = match self.symbolizer.read().await.lookup_symbol(addr).await {
            Ok(frame) => frame,
            Err(err) => {
                log::warn!("Failed to find frame for addr: {addr:x}, err: {err}");
                SymbolizedFrame {
                    symbol_name: "unknown".to_owned(),
                    library_name: Arc::new("unknown".to_owned()),
                    inlined: Vec::new(),
                    offset: 0,
                }
            }
        };
        self.localizer.process_symbolized_frame(addr, frame)
    }
}
