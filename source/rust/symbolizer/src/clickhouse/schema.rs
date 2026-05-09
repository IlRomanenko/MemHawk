use clickhouse::Row;
use serde::{Deserialize, Serialize};
use serde_repr::{Deserialize_repr, Serialize_repr};
use std::fmt::Debug;
use time::OffsetDateTime;

#[derive(Clone, Copy, Serialize_repr, Deserialize_repr)]
#[repr(i8)]
pub enum ValueType {
    ActiveSize = 1,
    ActiveCount = 2,
    TotalSize = 3,
    TotalCount = 4,
    Overhead = 5,
}

impl From<memhawk_core::proto::schema::ValueType> for ValueType {
    fn from(value: memhawk_core::proto::schema::ValueType) -> Self {
        match value {
            memhawk_core::proto::schema::ValueType::ActiveSize => Self::ActiveSize,
            memhawk_core::proto::schema::ValueType::ActiveCount => Self::ActiveCount,
            memhawk_core::proto::schema::ValueType::TotalSize => Self::TotalSize,
            memhawk_core::proto::schema::ValueType::TotalCount => Self::TotalCount,
            memhawk_core::proto::schema::ValueType::Overhead => Self::Overhead,
        }
    }
}

#[derive(Row, Serialize, Deserialize, Clone)]
pub struct RawDataRow {
    // Process selectors
    pub process_id: i32,

    // Frame data
    #[serde(with = "clickhouse::serde::time::datetime64::nanos")]
    pub timestamp: OffsetDateTime, // DateTime64(9)
    pub leaf_node_id: u32,

    // Actual memory profiling data
    pub value_type: ValueType,
    pub value: i64,
}

#[derive(Row, Serialize, Deserialize, Clone)]
pub struct TimeseriesRow {
    // Process selectors
    pub process_id: i32,

    // Frame data
    #[serde(with = "clickhouse::serde::time::datetime64::nanos")]
    pub timestamp: OffsetDateTime, // DateTime64(9)
    pub label_id: u32,

    pub value_type: ValueType,

    // Actual memory profiling data
    pub self_value: i64,
    pub total_value: i64,
}

#[derive(Row, Serialize, Deserialize, Clone)]
pub struct StacktraceRow {
    // Process selectors
    pub process_id: i32,

    // Stacktraces data
    pub leaf_node_id: u32,
    pub path: Vec<u32>,
}

#[derive(Row, Serialize, Deserialize, Clone)]
pub struct LocalizedNameRow {
    // Process selectors
    pub process_id: i32,

    // Stacktraces data
    pub label_id: u32,

    pub label: String,
    pub location: String,
    pub library: String,
    pub frame_offset: u64,
}

pub struct UpdateData {
    pub raw_data: Vec<RawDataRow>,
    pub timeseries: Vec<TimeseriesRow>,
    pub localized_names: Vec<LocalizedNameRow>,
    pub stacktraces: Vec<StacktraceRow>,
}

impl Debug for UpdateData {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("UpdateData")
            .field("raw_data", &self.raw_data.len())
            .field("timeseries", &self.timeseries.len())
            .field("localized_names", &self.localized_names.len())
            .field("stacktraces", &self.stacktraces.len())
            .finish()
    }
}
