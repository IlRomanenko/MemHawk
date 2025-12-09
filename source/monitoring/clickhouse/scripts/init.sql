CREATE DATABASE IF NOT EXISTS profiling;

CREATE TABLE IF NOT EXISTS profiling.profiles
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Frame data
    timestamp DateTime64(9),
    node_id UInt32,
    label_id UInt32,

-- Actual memory profiling data
    self_active_size Int64,
    self_active_count Int64,
    self_overhead Int64,
    self_total_size UInt64,
    self_total_count UInt64,

    total_active_size Int64,
    total_active_count Int64,
    total_overhead Int64,
    total_total_size UInt64,
    total_total_count UInt64,
)
ENGINE = MergeTree()
ORDER BY (process_id, node_id, timestamp);

CREATE TABLE IF NOT EXISTS profiling.stacktraces
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Stacktrace
    label_id UInt32,
    node_id UInt32, -- last node in node_path
    path Array(UInt32),
)
ENGINE = MergeTree()
ORDER BY (process_id, path);

CREATE TABLE IF NOT EXISTS profiling.localized_name
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Stacktraces data
    label_id UInt32,

    label String,
    location String,
    library String,
    frame_offset UInt64, -- for debugging purpose
)
ENGINE = MergeTree()
ORDER BY (process_id, label_id);
