use anyhow::bail;
use bitcode::{Decode, Encode};
use rustc_hash::{FxHashMap, FxHashSet};
use std::{cell::RefCell, rc::Rc, sync::Arc};
use time::OffsetDateTime;
use tokio::sync::RwLock;

use crate::{
    db::click_client::{ClickhouseClient, LocalizedNameRow, ProfileRow, StacktraceRow},
    graph::{self, Graph, NodeId},
    localizer::{
        self, FrameLocalizer, LocalizedFrameId, LocalizedName, MEMHAWK_ROOT_LOCALIZED_ID,
        OnFrameLocalized, UNKNOWN_LOCALIZED_ID, VecRange,
    },
    protos::{self, AllocSummary},
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

    localizer_state: localizer::RestorableState,
    symbolizer_state: symbolizer::RestorableState,
    graph_state: graph::RestorableState,

    ptr_id_to_addr_map: FxHashMap<u32, u64>,
    trace_id_to_leaf_id: FxHashMap<TraceId, NodeId>,
    written_stacktraces: FxHashSet<NodeId>,

    last_processed_timestamp: u64,
}

pub struct Processor {
    process_id: i32,

    click: Arc<ClickhouseClient>,

    symbolizer: Arc<RwLock<Symbolizer>>,
    localizer_sub: Rc<RefCell<FramesSubscription>>,
    localizer: FrameLocalizer,

    ptr_id_to_addr_map: FxHashMap<u32, u64>,

    symbolized_graph: Graph, //  graph after symbolyzer for frames

    trace_id_to_leaf_id: FxHashMap<TraceId, NodeId>,
    written_stacktraces: FxHashSet<NodeId>,

    modified_nodes: FxHashSet<NodeId>,

    last_processed_timestamp: u64,
}

impl Processor {
    pub fn new(process_id: i32) -> Self {
        let frames_sub = Rc::new(RefCell::new(FramesSubscription::new()));
        let click = ClickhouseClient::new();
        Self {
            process_id,
            click: Arc::new(click),
            symbolizer: Arc::new(RwLock::new(Symbolizer::new())),
            localizer_sub: frames_sub.clone(),
            localizer: FrameLocalizer::new(frames_sub),
            ptr_id_to_addr_map: FxHashMap::default(),
            symbolized_graph: Graph::new(),
            trace_id_to_leaf_id: FxHashMap::default(),
            written_stacktraces: FxHashSet::default(),
            modified_nodes: FxHashSet::default(),
            last_processed_timestamp: 0,
        }
    }

    pub async fn restore(state: RestorableState) -> anyhow::Result<Self> {
        let frames_sub = Rc::new(RefCell::new(FramesSubscription::new()));
        let click = ClickhouseClient::new();
        let symbolizer = Symbolizer::restore(state.symbolizer_state).await;

        let processor = Self {
            process_id: state.process_id,
            click: Arc::new(click),
            symbolizer: Arc::new(RwLock::new(symbolizer)),
            localizer_sub: frames_sub.clone(),
            localizer: FrameLocalizer::restore(state.localizer_state, frames_sub),
            ptr_id_to_addr_map: state.ptr_id_to_addr_map,
            symbolized_graph: Graph::restore(state.graph_state),
            trace_id_to_leaf_id: state.trace_id_to_leaf_id,
            written_stacktraces: state.written_stacktraces,
            modified_nodes: FxHashSet::default(),
            last_processed_timestamp: state.last_processed_timestamp,
        };
        Ok(processor)
    }

    pub async fn save(&self) -> RestorableState {
        RestorableState {
            process_id: self.process_id,
            localizer_state: self.localizer.save(),
            symbolizer_state: self.symbolizer.read().await.save(),
            graph_state: self.symbolized_graph.save(),
            ptr_id_to_addr_map: self.ptr_id_to_addr_map.clone(),
            trace_id_to_leaf_id: self.trace_id_to_leaf_id.clone(),
            written_stacktraces: self.written_stacktraces.clone(),
            last_processed_timestamp: self.last_processed_timestamp,
        }
    }

    pub async fn process(&mut self, snapshot: &protos::Snapshot) -> anyhow::Result<()> {
        log::info!(
            "Processing : {}",
            OffsetDateTime::from_unix_timestamp_nanos(snapshot.timestamp as i128)?
        );

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
            self.symbolizer
                .write()
                .await
                .update_symbols(&snapshot.loaded_so)
                .await;
        }
        self.process_stacktraces(snapshot).await;
        self.process_allocations(snapshot).await?;

        self.process_modifications(snapshot.timestamp).await?;
        Ok(())
    }

    async fn process_modifications(&mut self, timestamp: u64) -> anyhow::Result<()> {
        let names = self.localizer_sub.borrow_mut().consume(self.process_id);
        let (profiles, stacktraces) = self.consume_profiles(timestamp)?;

        log::info!("Names: {}, profiles: {}, stacktraces: {}", names.len(), profiles.len(), stacktraces.len());

        let names_click = self.click.clone();
        let names_future =
            tokio::spawn(async move { names_click.insert_localized_names(&names).await });
        let profiles_click = self.click.clone();
        let profiles_future =
            tokio::spawn(async move { profiles_click.insert_profiles(profiles).await });
        let stacktraces_click = self.click.clone();
        let stacktraces_future =
            tokio::spawn(async move { stacktraces_click.insert_stacktraces(stacktraces).await });
        // todo: remove this ugly mess
        let _ = tokio::try_join!(names_future, profiles_future, stacktraces_future)?;
        Ok(())
    }

    fn consume_profiles(
        &mut self,
        timestamp: u64,
    ) -> anyhow::Result<(Vec<ProfileRow>, Vec<StacktraceRow>)> {
        let mut profiles = Vec::new();
        let mut stacktraces = Vec::new();

        self.modified_nodes = self.symbolized_graph.process_postponed_updates();
        if self.modified_nodes.is_empty() {
            self.modified_nodes.insert(NodeId::from(0)); // add root node
        }

        log::info!("Modified nodes: {}", self.modified_nodes.len());

        for node_id in self.modified_nodes.iter().copied() {
            let node = self.symbolized_graph.inspect(node_id);
            let self_value: AllocSummary = match &node.self_value {
                Some(alloc_summary) => **alloc_summary,
                None => AllocSummary::default(),
            };
            profiles.push(ProfileRow {
                process_id: self.process_id,
                timestamp: OffsetDateTime::from_unix_timestamp_nanos(timestamp as i128)?,
                node_id: node_id.into(),
                label_id: node.key.into(),
                self_active_size: self_value.size,
                self_active_count: self_value.active,
                self_overhead: self_value.overhead,
                self_total_size: self_value.total_bytes,
                self_total_count: self_value.total_count,
                total_active_size: node.total_value.size,
                total_active_count: node.total_value.active,
                total_overhead: node.total_value.overhead,
                total_total_size: node.total_value.total_bytes,
                total_total_count: node.total_value.total_count,
            });

            if !self.written_stacktraces.contains(&node_id) {
                self.written_stacktraces.insert(node_id);

                let node_key = node.key;
                let mut path = Vec::new();
                self.symbolized_graph.backtrace(node_id, &mut path);

                stacktraces.push(StacktraceRow {
                    process_id: self.process_id,
                    label_id: node_key.into(),
                    node_id: node_id.into(),
                    path: path.into_iter().map(|x| x.into()).collect::<Vec<_>>(),
                });
            }
        }
        self.modified_nodes.clear();
        Ok((profiles, stacktraces))
    }

    async fn process_allocations(&mut self, snapshot: &protos::Snapshot) -> anyhow::Result<()> {
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
        log::info!("Processor::trace_id_to_leaf_id.len(): {}", self.trace_id_to_leaf_id.len());
        log::info!("Processor::ptr_id_to_addr_map.len(): {}", self.ptr_id_to_addr_map.len());
        self.localizer.stats();
        self.symbolized_graph.stats();
    }

    fn process_alloc_summary(
        &mut self,
        summary: &protos::TracedAllocSummary,
    ) -> anyhow::Result<()> {
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
        self.symbolized_graph.postpone_update(leaf_id, actual);
        Ok(())
    }

    async fn symbolize_new_addrs(&mut self, snapshot: &protos::Snapshot) {
        // todo: Rewrite in more idiomatic style
        let mut tasks = Vec::new();
        for elem in snapshot.ptr_ids.iter() {
            let symbolizer = Arc::clone(&self.symbolizer);
            let addr = elem.ptr_addr - 1;
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
                    log::debug!("Failed to find frame for addr: {addr:x}, err: {err}");
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

    async fn process_stacktraces(&mut self, snapshot: &protos::Snapshot) {
        for pair in snapshot.ptr_ids.iter() {
            self.ptr_id_to_addr_map.insert(pair.ptr_id, pair.ptr_addr - 1);
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
