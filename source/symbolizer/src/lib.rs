use crate::{clickhouse::schema::ValueType, proto::schema::AllocSummary};

pub mod clickhouse;
pub mod graph;
pub mod localizer;
pub mod postgres;
pub mod processor;
pub mod proto;
pub mod symbolizer;

fn value_selector(summary: &AllocSummary, value_type: ValueType) -> i64 {
    match value_type {
        ValueType::ActiveSize => summary.size,
        ValueType::ActiveCount => summary.active,
        ValueType::TotalCount => summary.total_count as i64,
        ValueType::TotalSize => summary.total_bytes as i64,
    }
}
