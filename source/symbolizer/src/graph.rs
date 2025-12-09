use std::ops::{Index, IndexMut};

use bitcode::{Decode, Encode};
use derive_more::{Add, From, Into};
use rustc_hash::{FxHashMap, FxHashSet};

use crate::{
    localizer::{LocalizedFrameId, ROOT_LOCALIZED_ID},
    protos::AllocSummary,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq, From, Into, Hash, Add, Encode, Decode)]
pub struct NodeId(u32);

#[derive(Debug, Clone, Encode, Decode)]
pub struct GraphNode {
    pub key: LocalizedFrameId,
    pub parent: NodeId,
    pub level: u32,
    pub self_value: Option<Box<AllocSummary>>,
    pub total_value: AllocSummary,
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

#[derive(Encode, Decode)]
pub struct RestorableState {
    nodes: Vec<GraphNode>,
    edges: FxHashMap<GraphEdge, NodeId>,
    // last_node_id: NodeId,
}

pub struct Graph {
    nodes: Vec<GraphNode>,
    edges: FxHashMap<GraphEdge, NodeId>,

    postponed_updates: FxHashMap<u32, FxHashMap<NodeId, AllocSummary>>,
    // last_node_id: NodeId,
}

impl Graph {
    pub fn new() -> Self {
        let mut graph = Graph {
            nodes: Vec::new(),
            edges: FxHashMap::default(),
            postponed_updates: FxHashMap::default(),
            // last_node_id: NodeId(0), // NodeId(0) = root node
        };
        // push root node
        graph.nodes.push(GraphNode {
            key: ROOT_LOCALIZED_ID,
            parent: NodeId(0),
            level: 0,
            self_value: None,
            total_value: AllocSummary::default(),
        });
        graph
    }

    pub fn save(&self) -> RestorableState {
        RestorableState {
            nodes: self.nodes.clone(),
            edges: self.edges.clone(),
            // last_node_id: self.last_node_id,
        }
    }

    pub fn restore(state: RestorableState) -> Self {
        Self {
            nodes: state.nodes,
            edges: state.edges,
            postponed_updates: FxHashMap::default(),
            // last_node_id: state.last_node_id,
        }
    }

    pub fn stats(&self) {
        log::info!("Graph::nodes.len(): {}", self.nodes.len());
        log::info!("Graph::edges.len(): {}", self.edges.len());
    }

    pub fn construct_path(&mut self, trace: &[LocalizedFrameId]) -> NodeId {
        let mut node_id = NodeId(0);
        for trace_id in trace {
            let edge = GraphEdge {
                from: node_id,
                by: *trace_id,
            };
            node_id = match self.edges.get(&edge) {
                Some(value) => *value,
                None => {
                    let child_id = self.get_next_node_id();
                    self.edges.insert(edge, child_id);
                    self.nodes.push(
                        // child_id,
                        GraphNode {
                            key: *trace_id,
                            parent: node_id,
                            level: self.nodes[node_id].level + 1,
                            self_value: None,
                            total_value: AllocSummary::default(),
                        },
                    );
                    child_id
                }
            };
        }
        node_id
    }

    fn get_next_node_id(&mut self) -> NodeId {
        (self.nodes.len() as u32).into()
        // self.last_node_id = self.last_node_id + NodeId(1);
        // self.last_node_id
    }

    pub fn postpone_update(&mut self, node_id: NodeId, summary: AllocSummary) {
        let level = self.nodes[node_id].level;
        let level_map = self.postponed_updates.entry(level).or_default();
        level_map
            .entry(node_id)
            .and_modify(|x| *x += summary)
            .or_insert(summary);
    }

    pub fn process_postponed_updates(&mut self) -> FxHashSet<NodeId> {
        let mut modified_nodes = FxHashSet::default();

        let max_level = match self.postponed_updates.keys().max() {
            Some(level) => *level,
            None => return modified_nodes,
        };

        let mut cur_accumulated_diff = FxHashMap::default();
        let mut next_accumulated_diff = FxHashMap::default();

        for level in (0..max_level + 1).rev() {
            if let Some((_, level_map)) = self.postponed_updates.remove_entry(&level) {
                for (node_id, summary) in level_map {
                    let node = self.inspect(node_id);

                    // update self value for node
                    let current = node.self_value.get_or_insert_default();
                    let diff = summary - **current;
                    **current = summary;

                    // accumulate diff to change nodes on path
                    cur_accumulated_diff
                        .entry(node_id)
                        .and_modify(|x| *x += diff)
                        .or_insert(diff);
                }
            }

            for (node_id, diff) in cur_accumulated_diff {
                modified_nodes.insert(node_id);

                let node = self.inspect(node_id);
                node.total_value += diff;

                next_accumulated_diff
                    .entry(node.parent)
                    .and_modify(|x| *x += diff)
                    .or_insert(diff);
            }
            cur_accumulated_diff = next_accumulated_diff;
            next_accumulated_diff = FxHashMap::default();
        }

        modified_nodes
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

    pub fn inspect(&mut self, node_id: NodeId) -> &mut GraphNode {
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
