use std::io::BufRead;

use memhawk_core::{
    graph::Graph,
    localizer::LocalizedFrameId,
    proto::schema::{AllocSummary, ValueType},
};

fn get_top_n() -> anyhow::Result<()> {
    let mut graph = Graph::new();

    let stdin = std::io::stdin().lock();
    for line in stdin.lines() {
        let line = line?;
        let (diff, path) = line
            .split_once('\t')
            .ok_or(anyhow::format_err!("Failed to split row"))?;

        let diff = str::parse::<i64>(diff)?;
        let path = path
            .strip_prefix('[')
            .ok_or(anyhow::format_err!("Incorrect start for line: {}", &line))?
            .strip_suffix(']')
            .ok_or(anyhow::format_err!("Incorrect end for line: {}", &line))?
            .split(',')
            .into_iter()
            .map(|x| str::parse::<u32>(x))
            .flatten()
            .map(|x| LocalizedFrameId::from(x))
            .collect::<Vec<_>>();

        let leaf_node_id = graph.construct_path(&path);
        graph.postpone_update(
            leaf_node_id,
            AllocSummary {
                size: diff,
                active: 0,
                overhead: 0,
                total_count: 0,
                total_bytes: 0,
            },
        );
    }
    graph.process_postponed_updates();
    let nodes = graph.get_top(1000, ValueType::ActiveSize);

    for (order_id, node) in nodes.iter().enumerate() {
        let depth = graph.inspect(node.node_id).level;
        let key: u32 = graph.inspect(node.node_id).key.into();
        println!(
            "{}\t{}\t{}\t{}\t{}",
            order_id, key, depth, node.total_value, node.self_value
        );
    }
    Ok(())
}

fn main() {
    let _ = get_top_n();
}
