use std::{
    collections::BinaryHeap,
    ops::{Index, IndexMut},
};

use bitcode::{Decode, Encode};
use derive_more::{Add, From, Into};
use rustc_hash::{FxHashMap, FxHashSet};

use crate::{
    clickhouse::schema::ValueType,
    localizer::{LocalizedFrameId, ROOT_LOCALIZED_ID},
    proto::schema::AllocSummary,
    value_selector,
};

#[derive(
    Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, From, Into, Hash, Add, Encode, Decode,
)]
pub struct NodeId(u32);

#[derive(Debug, Clone, Copy, PartialEq, Eq, From, Into, Encode, Decode)]
pub struct ChildElemId(u32);

#[derive(Debug, Clone, Copy, Encode, Decode)]
struct ChildList {
    child: NodeId,
    next: Option<ChildElemId>,
}

impl Index<ChildElemId> for Vec<ChildList> {
    type Output = ChildList;

    fn index(&self, index: ChildElemId) -> &Self::Output {
        &self[index.0 as usize]
    }
}
impl IndexMut<ChildElemId> for Vec<ChildList> {
    fn index_mut(&mut self, index: ChildElemId) -> &mut Self::Output {
        &mut self[index.0 as usize]
    }
}

#[derive(Debug, Clone, Encode, Decode)]
pub struct GraphNode {
    pub key: LocalizedFrameId,
    pub parent: NodeId,
    pub level: u32,
    pub is_leaf: bool,
    pub first_on_path: bool,
    pub children: Option<ChildElemId>,
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

    fn accumulate(&mut self, diff: &AllocSummary, diff_type: AccumulateDiffType) {
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
    children: Vec<ChildList>,
    localized_frame_agg: FxHashMap<LocalizedFrameId, AggregatedAllocSummary>,
    postponed_updates: FxHashMap<u32, FxHashMap<NodeId, AllocSummary>>,
    new_nodes: FxHashSet<NodeId>,
    new_leaf_nodes: FxHashSet<NodeId>,
    modified_nodes: FxHashSet<NodeId>,
    modified_frames: FxHashSet<LocalizedFrameId>,
}

pub struct AggregatedModifications {
    pub new_nodes: FxHashSet<NodeId>,
    pub new_leaf_nodes: FxHashSet<NodeId>,
    pub modified_nodes: FxHashSet<NodeId>,
    pub modified_frames: FxHashMap<LocalizedFrameId, AggregatedAllocSummary>,
}

pub struct NodeWithValue {
    pub node_id: NodeId,
    pub self_value: i64,
    pub total_value: i64,
}

pub struct Graph {
    nodes: Vec<GraphNode>,
    edges: FxHashMap<GraphEdge, NodeId>,
    children: Vec<ChildList>,
    localized_frame_agg: FxHashMap<LocalizedFrameId, AggregatedAllocSummary>,
    postponed_updates: FxHashMap<u32, FxHashMap<NodeId, AllocSummary>>,
    new_nodes: FxHashSet<NodeId>,
    new_leaf_nodes: FxHashSet<NodeId>,
    modified_nodes: FxHashSet<NodeId>,
    modified_frames: FxHashSet<LocalizedFrameId>,
}

impl Graph {
    pub fn new() -> Self {
        let mut graph = Graph {
            nodes: Vec::new(),
            edges: FxHashMap::default(),
            children: Vec::new(),
            localized_frame_agg: FxHashMap::default(),
            postponed_updates: FxHashMap::default(),

            new_nodes: FxHashSet::default(),
            new_leaf_nodes: FxHashSet::default(),
            modified_nodes: FxHashSet::default(),
            modified_frames: FxHashSet::default(),
        };
        // push root node
        graph.nodes.push(GraphNode {
            key: ROOT_LOCALIZED_ID,
            parent: NodeId(0),
            level: 0,
            children: None,
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
            children: self.children.clone(),
            localized_frame_agg: self.localized_frame_agg.clone(),
            postponed_updates: self.postponed_updates.clone(),
            new_nodes: self.new_nodes.clone(),
            new_leaf_nodes: self.new_leaf_nodes.clone(),
            modified_nodes: self.modified_nodes.clone(),
            modified_frames: self.modified_frames.clone(),
        }
    }

    pub fn restore(state: RestorableState) -> Self {
        Self {
            nodes: state.nodes,
            edges: state.edges,
            children: state.children,
            localized_frame_agg: state.localized_frame_agg,
            postponed_updates: state.postponed_updates,
            new_nodes: state.new_nodes,
            new_leaf_nodes: state.new_leaf_nodes,
            modified_nodes: state.modified_nodes,
            modified_frames: state.modified_frames,
        }
    }

    pub fn get_top(&self, count: usize, value_type: ValueType) -> Vec<NodeWithValue> {
        let mut sorted_nodes = BinaryHeap::default();
        let mut selected_nodes_with_order = FxHashMap::default();

        let root = NodeId::from(0);
        sorted_nodes.push((
            value_selector(&self.nodes[root].aggregated.total_value, value_type),
            root,
        ));

        while selected_nodes_with_order.len() < count {
            let (_, node_id) = match sorted_nodes.pop() {
                Some(pair) => pair,
                None => break,
            };
            selected_nodes_with_order.insert(node_id, selected_nodes_with_order.len());

            let mut child_iter = self.nodes[node_id].children;
            while let Some(child_elem_id) = child_iter {
                let child_node_list = self.children[child_elem_id];
                let child_node_id = child_node_list.child;
                let child_node_value = value_selector(
                    &self.nodes[child_node_id].aggregated.total_value,
                    value_type,
                );
                if self.nodes[child_node_id].parent != node_id {
                    log::error!("Invalid child");
                }
                sorted_nodes.push((child_node_value, child_node_id));

                child_iter = child_node_list.next;
            }
        }
        sorted_nodes.clear();

        let mut result_order = Vec::new();
        self.dfs(
            NodeId::from(0),
            &selected_nodes_with_order,
            &mut result_order,
        );
        let transformed = result_order
            .into_iter()
            .map(|node_id| {
                let node = self.inspect(node_id);
                NodeWithValue {
                    node_id: node_id,
                    self_value: node
                        .aggregated
                        .self_value
                        .as_ref()
                        .map_or(0, |boxed| value_selector(boxed, value_type)),
                    total_value: value_selector(&node.aggregated.total_value, value_type),
                }
            })
            .collect::<Vec<_>>();
        transformed
    }

    fn dfs(&self, root: NodeId, selected: &FxHashMap<NodeId, usize>, order: &mut Vec<NodeId>) {
        order.push(root);
        let mut candidates = Vec::new();

        let mut child_iter = self.nodes[root].children;
        while let Some(child_elem_id) = child_iter {
            let child_node_list = self.children[child_elem_id];
            let child_node_id = child_node_list.child;

            if let Some(order_id) = selected.get(&child_node_id) {
                candidates.push((*order_id, child_node_id));
            }
            child_iter = child_node_list.next;
        }
        // sort childs by order
        candidates.sort();
        for (_, node_id) in candidates {
            self.dfs(node_id, selected, order);
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
            children: None,
            is_leaf: false,
            first_on_path: is_first_on_path,
            aggregated: AggregatedAllocSummary::default(),
        });
        let parent_child_list_head = self.nodes[parent].children;
        let new_child_elem_id = ChildElemId::from(self.children.len() as u32);
        self.children.push(ChildList {
            child: child_id,
            next: parent_child_list_head,
        });
        self.nodes[parent].children = Some(new_child_elem_id);
        self.new_nodes.insert(child_id);
        child_id
    }

    fn get_next_node_id(&mut self) -> NodeId {
        (self.nodes.len() as u32).into()
    }

    pub fn postpone_update(&mut self, node_id: NodeId, summary: AllocSummary) {
        let level = self.nodes[node_id].level;
        let level_map = self.postponed_updates.entry(level).or_default();
        level_map
            .entry(node_id)
            .and_modify(|x| *x += summary)
            .or_insert(summary);
    }

    fn aggregate_on_frame(
        &mut self,
        node_id: NodeId,
        diff: AllocSummary,
        diff_type: AccumulateDiffType,
    ) {
        let node = self.inspect(node_id);
        let node_key = node.key;
        self.localized_frame_agg
            .entry(node_key)
            .and_modify(|x| x.accumulate(&diff, diff_type))
            .or_insert(AggregatedAllocSummary::from_summary(diff, diff_type));
        self.modified_frames.insert(node_key);
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
                for (node_id, summary) in level_map {
                    let node = self.inspect_mut(node_id);

                    // update self value for node
                    let current = node.aggregated.self_value.get_or_insert_default();
                    let diff = summary - **current;
                    **current = summary;

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
                self.modified_nodes.insert(node_id);

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
            new_nodes: FxHashSet::default(),
            new_leaf_nodes: FxHashSet::default(),
            modified_nodes: FxHashSet::default(),
            modified_frames: FxHashMap::default(),
        };
        std::mem::swap(&mut result.new_nodes, &mut self.new_nodes);
        std::mem::swap(&mut result.new_leaf_nodes, &mut self.new_leaf_nodes);
        std::mem::swap(&mut result.modified_nodes, &mut self.modified_nodes);
        for frame_id in self.modified_frames.drain() {
            result.modified_frames.insert(
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
}
