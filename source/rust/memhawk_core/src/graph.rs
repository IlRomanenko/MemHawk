use std::ops::{Index, IndexMut};

use bitcode::{Decode, Encode};
use derive_more::{Add, From, Into};
use rustc_hash::{FxHashMap, FxHashSet};

use crate::{
    localizer::{LocalizedFrameId, ROOT_LOCALIZED_ID},
    proto::schema::{AllocDiff, AllocSummary},
};

#[derive(
    Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, From, Into, Hash, Add, Encode, Decode,
)]
pub struct NodeId(u32);

#[derive(Debug, Clone, Encode, Decode)]
pub struct GraphNode {
    pub key: LocalizedFrameId,
    pub parent: NodeId,
    pub level: u32,
    pub is_leaf: bool,
    pub first_on_path: bool,
    pub aggregated: AggregatedAllocSummary,
}

impl Index<NodeId> for Vec<GraphNode> {
    type Output = GraphNode;

    fn index(&self, index: NodeId) -> &Self::Output {
        &self[index.0 as usize]
    }
}
impl IndexMut<NodeId> for Vec<GraphNode> {
    fn index_mut(&mut self, index: NodeId) -> &mut Self::Output {
        &mut self[index.0 as usize]
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Encode, Decode)]
pub struct GraphEdge {
    from: NodeId,
    by: LocalizedFrameId,
}

impl GraphEdge {
    pub fn new(from: u32, by: u32) -> Self {
        Self {
            from: NodeId::from(from),
            by: LocalizedFrameId::from(by),
        }
    }
}

#[derive(Debug, Clone, Copy)]
enum AccumulateDiffType {
    Current,
    Total,
}

#[derive(Debug, Clone, Encode, Decode, Default)]
pub struct AggregatedAllocSummary {
    pub self_value: Option<Box<AllocSummary>>,
    pub total_value: AllocSummary,
}

impl AggregatedAllocSummary {
    fn from_summary(summary: AllocSummary, diff_type: AccumulateDiffType) -> Self {
        match diff_type {
            AccumulateDiffType::Current => Self {
                self_value: Some(Box::new(summary)),
                total_value: AllocSummary::default(),
            },
            AccumulateDiffType::Total => Self {
                self_value: None,
                total_value: summary,
            },
        }
    }

    fn accumulate(&mut self, diff: &AllocDiff, diff_type: AccumulateDiffType) {
        match diff_type {
            AccumulateDiffType::Current => {
                let self_value = self.self_value.get_or_insert_default();
                **self_value += *diff;
            }
            AccumulateDiffType::Total => {
                self.total_value += *diff;
            }
        }
    }
}

#[derive(Encode, Decode)]
pub struct RestorableState {
    nodes: Vec<GraphNode>,
    edges: FxHashMap<GraphEdge, NodeId>,
    localized_frame_agg: FxHashMap<LocalizedFrameId, AggregatedAllocSummary>,
    modified_frame_agg: FxHashSet<LocalizedFrameId>,
    postponed_updates: FxHashMap<u32, FxHashMap<NodeId, AllocDiff>>,
    modified_self_nodes: FxHashSet<NodeId>,
    new_leaf_nodes: FxHashSet<NodeId>,
}

pub struct AggregatedModifications {
    pub new_leaf_nodes: FxHashSet<NodeId>,
    pub frames: FxHashMap<LocalizedFrameId, AggregatedAllocSummary>,
    pub modified_self_nodes: FxHashMap<NodeId, AllocSummary>,
}

pub struct NodeWithValue {
    pub node_id: NodeId,
    pub self_value: i64,
    pub total_value: i64,
}

pub struct Graph {
    nodes: Vec<GraphNode>,
    edges: FxHashMap<GraphEdge, NodeId>,
    localized_frame_agg: FxHashMap<LocalizedFrameId, AggregatedAllocSummary>,
    modified_frame_agg: FxHashSet<LocalizedFrameId>,
    postponed_updates: FxHashMap<u32, FxHashMap<NodeId, AllocDiff>>,
    modified_self_nodes: FxHashSet<NodeId>,
    new_leaf_nodes: FxHashSet<NodeId>,
}

impl Graph {
    pub fn new() -> Self {
        let mut graph = Graph {
            nodes: Vec::new(),
            edges: FxHashMap::default(),
            localized_frame_agg: FxHashMap::default(),
            modified_frame_agg: FxHashSet::default(),
            postponed_updates: FxHashMap::default(),
            modified_self_nodes: FxHashSet::default(),

            new_leaf_nodes: FxHashSet::default(),
        };
        // push root node
        graph.nodes.push(GraphNode {
            key: ROOT_LOCALIZED_ID,
            parent: NodeId(0),
            level: 0,
            is_leaf: false,
            first_on_path: true,
            aggregated: AggregatedAllocSummary::default(),
        });
        graph
    }

    pub fn save(&self) -> RestorableState {
        RestorableState {
            nodes: self.nodes.clone(),
            edges: self.edges.clone(),
            localized_frame_agg: self.localized_frame_agg.clone(),
            modified_frame_agg: self.modified_frame_agg.clone(),
            postponed_updates: self.postponed_updates.clone(),
            modified_self_nodes: self.modified_self_nodes.clone(),
            new_leaf_nodes: self.new_leaf_nodes.clone(),
        }
    }

    pub fn restore(state: RestorableState) -> Self {
        Self {
            nodes: state.nodes,
            edges: state.edges,
            localized_frame_agg: state.localized_frame_agg,
            modified_frame_agg: state.modified_frame_agg,
            postponed_updates: state.postponed_updates,
            modified_self_nodes: state.modified_self_nodes,
            new_leaf_nodes: state.new_leaf_nodes,
        }
    }

    pub fn stats(&self) {
        log::info!("Graph::nodes.len(): {}", self.nodes.len());
        log::info!("Graph::edges.len(): {}", self.edges.len());
    }

    pub fn construct_path(&mut self, trace: &[LocalizedFrameId]) -> NodeId {
        let mut frames_set = FxHashSet::default();

        let mut node_id = NodeId(0);
        for trace_id in trace {
            let is_first_on_path = !frames_set.contains(trace_id);
            frames_set.insert(*trace_id);

            let edge = GraphEdge {
                from: node_id,
                by: *trace_id,
            };
            node_id = match self.edges.get(&edge) {
                Some(value) => *value,
                None => self.add_node(node_id, edge, is_first_on_path),
            };
        }
        if !self.inspect(node_id).is_leaf {
            self.new_leaf_nodes.insert(node_id);
            self.inspect_mut(node_id).is_leaf = true;
        }
        node_id
    }

    fn add_node(&mut self, parent: NodeId, edge: GraphEdge, is_first_on_path: bool) -> NodeId {
        let child_id = self.get_next_node_id();
        self.edges.insert(edge, child_id);
        self.nodes.push(GraphNode {
            key: edge.by,
            parent: parent,
            level: self.nodes[parent].level + 1,
            is_leaf: false,
            first_on_path: is_first_on_path,
            aggregated: AggregatedAllocSummary::default(),
        });
        child_id
    }

    fn get_next_node_id(&mut self) -> NodeId {
        (self.nodes.len() as u32).into()
    }

    pub fn postpone_diff_update(&mut self, node_id: NodeId, alloc_diff: AllocDiff) {
        let level = self.nodes[node_id].level;
        let level_map = self.postponed_updates.entry(level).or_default();
        level_map
            .entry(node_id)
            .and_modify(|x| *x += alloc_diff)
            .or_insert(alloc_diff);
        self.modified_self_nodes.insert(node_id);
    }

    fn aggregate_on_frame(
        &mut self,
        node_id: NodeId,
        diff: AllocDiff,
        diff_type: AccumulateDiffType,
    ) {
        let node = self.inspect(node_id);
        let node_key = node.key;
        self.localized_frame_agg
            .entry(node_key)
            .and_modify(|x| x.accumulate(&diff, diff_type))
            .or_insert(AggregatedAllocSummary::from_summary(
                AllocSummary::from(diff),
                diff_type,
            ));
        self.modified_frame_agg.insert(node_key);
    }

    pub fn process_postponed_updates(&mut self) {
        let max_level = match self.postponed_updates.keys().max() {
            Some(level) => *level,
            None => return,
        };

        let mut cur_accumulated_diff = FxHashMap::default();
        let mut next_accumulated_diff = FxHashMap::default();

        for level in (0..max_level + 1).rev() {
            // process node self value update
            if let Some((_, level_map)) = self.postponed_updates.remove_entry(&level) {
                for (node_id, diff) in level_map {
                    let node = self.inspect_mut(node_id);

                    // update self value for node
                    let current = node.aggregated.self_value.get_or_insert_default();
                    **current += diff;

                    // accumulate diff to change nodes on path
                    cur_accumulated_diff
                        .entry(node_id)
                        .and_modify(|x| *x += diff)
                        .or_insert(diff);

                    if node.first_on_path {
                        self.aggregate_on_frame(node_id, diff, AccumulateDiffType::Current);
                    }
                }
            }

            // process node subtree value update
            for (node_id, diff) in cur_accumulated_diff {
                let node = self.inspect_mut(node_id);
                node.aggregated.total_value += diff;

                next_accumulated_diff
                    .entry(node.parent)
                    .and_modify(|x| *x += diff)
                    .or_insert(diff);

                if node.first_on_path {
                    self.aggregate_on_frame(node_id, diff, AccumulateDiffType::Total);
                }
            }
            cur_accumulated_diff = next_accumulated_diff;
            next_accumulated_diff = FxHashMap::default();
        }
    }

    pub fn consume_modifications(&mut self) -> AggregatedModifications {
        let mut result = AggregatedModifications {
            new_leaf_nodes: FxHashSet::default(),
            frames: FxHashMap::default(),
            modified_self_nodes: FxHashMap::default(),
        };
        std::mem::swap(&mut result.new_leaf_nodes, &mut self.new_leaf_nodes);
        for node_id in self.modified_self_nodes.drain() {
            result.modified_self_nodes.insert(
                node_id,
                *(self.nodes[node_id].aggregated.self_value.clone().unwrap()),
            );
        }
        for frame_id in self.modified_frame_agg.drain() {
            result.frames.insert(
                frame_id,
                self.localized_frame_agg.get(&frame_id).unwrap().clone(),
            );
        }
        result
    }

    pub fn backtrace(&self, mut node_id: NodeId, path_nodes: &mut Vec<NodeId>) {
        let root_node_id = NodeId(0);
        path_nodes.clear();
        path_nodes.push(node_id);
        while node_id != root_node_id {
            let node = &self.nodes[node_id];
            node_id = node.parent;
            path_nodes.push(node_id);
        }
        path_nodes.reverse();
    }

    pub fn inspect(&self, node_id: NodeId) -> &GraphNode {
        &self.nodes[node_id]
    }

    pub fn inspect_mut(&mut self, node_id: NodeId) -> &mut GraphNode {
        &mut self.nodes[node_id]
    }
}

impl Default for Graph {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_construct() {
        let mut graph = Graph::new();

        let traces = vec![
            vec![1, 2, 3, 4, 5],
            vec![1, 6, 7],
            vec![1, 2, 3, 8],
            vec![1, 9],
        ];
        for trace in traces {
            graph.construct_path(
                &trace
                    .iter()
                    .map(|x| LocalizedFrameId::from(*x))
                    .collect::<Vec<_>>(),
            );
        }
        assert_eq!(graph.nodes.len(), 10);
    }

    #[test]
    fn test_backtrace() {
        let mut graph = Graph::new();

        let traces = vec![
            vec![1, 2, 3, 4, 5],
            vec![1, 6, 7],
            vec![1, 2, 3, 8],
            vec![1, 9],
        ];
        let mut leaf_nodes = Vec::new();
        for trace in traces {
            let leaf_id = graph.construct_path(
                &trace
                    .iter()
                    .map(|x| LocalizedFrameId::from(*x))
                    .collect::<Vec<_>>(),
            );
            leaf_nodes.push(leaf_id);
        }
        let mut path_nodes = Vec::new();
        for leaf_id in leaf_nodes.iter() {
            graph.backtrace(*leaf_id, &mut path_nodes);
            assert_eq!(*path_nodes.first().unwrap(), NodeId(0));
            assert_eq!(*path_nodes.last().unwrap(), *leaf_id);
        }
    }

    #[test]
    fn test_postpone_process_multiple_times() {
        let mut graph = Graph::new();
        let frame_id = LocalizedFrameId::from(1);
        let trace = vec![frame_id];
        let leaf_node = graph.construct_path(&trace);
        graph.postpone_diff_update(
            leaf_node,
            AllocDiff {
                size: 10,
                active: 2,
                overhead: 0,
                total_count: 2,
                total_bytes: 10,
            },
        );
        graph.process_postponed_updates();
        graph.postpone_diff_update(
            leaf_node,
            AllocDiff {
                size: 20,
                active: 4,
                overhead: 0,
                total_count: 4,
                total_bytes: 20,
            },
        );
        graph.process_postponed_updates();
        let res = graph.consume_modifications();
        assert_eq!(
            res.frames[&frame_id].total_value,
            AllocSummary {
                size: 30,
                active: 6,
                overhead: 0,
                total_count: 6,
                total_bytes: 30
            }
        );
    }

    #[test]
    fn test_multiple_postpone_single_process() {
        let mut graph = Graph::new();
        let frame_id = LocalizedFrameId::from(1);
        let trace = vec![frame_id];
        let leaf_node = graph.construct_path(&trace);
        graph.postpone_diff_update(
            leaf_node,
            AllocDiff {
                size: 10,
                active: 2,
                overhead: 0,
                total_count: 2,
                total_bytes: 10,
            },
        );
        graph.postpone_diff_update(
            leaf_node,
            AllocDiff {
                size: 20,
                active: 4,
                overhead: 0,
                total_count: 4,
                total_bytes: 20,
            },
        );
        graph.process_postponed_updates();
        let res = graph.consume_modifications();
        assert_eq!(
            res.frames[&frame_id].total_value,
            AllocSummary {
                size: 30,
                active: 6,
                overhead: 0,
                total_count: 6,
                total_bytes: 30
            }
        );
    }
}
