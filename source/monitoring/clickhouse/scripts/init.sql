CREATE DATABASE IF NOT EXISTS profiling;

use profiling;

CREATE TABLE IF NOT EXISTS raw_data
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Frame data
    timestamp DateTime64(9),
    leaf_node_id UInt32,

-- Actual memory profiling data
    active_size Int64 ,
    active_count Int64,
    overhead Int64,
    total_size UInt64,
    total_count UInt64,
)
ENGINE = MergeTree()
ORDER BY (process_id, leaf_node_id, timestamp);


-- Top 10k nodes from symbolized graph by value_type stored in dfs order
CREATE TABLE IF NOT EXISTS flamegraphs
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Frame data
    timestamp DateTime64(9),
    label_id UInt32,
    order_id UInt32, -- dfs order on nodes
    level UInt32,

    value_type Enum('active_size' = 1, 'active_count' = 2, 'total_size' = 3, 'total_count' = 4),

-- Actual memory profiling data
    self_value Int64, -- value associated with node
    total_value Int64, -- value associated with subtree
)
ENGINE = MergeTree()
ORDER BY (process_id, value_type, timestamp, order_id);

-- Aggregated value for each label -> self value and all subtree, for each path first occurence is used
CREATE TABLE IF NOT EXISTS timeseries
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Frame data
    timestamp DateTime64(9),
    label_id UInt32,

    value_type Enum('active_size' = 1, 'active_count' = 2, 'total_size' = 3, 'total_count' = 4),

-- Actual memory profiling data
    self_value Int64, -- value associated with node
    total_value Int64, -- value associated with subtree
)
ENGINE = MergeTree()
ORDER BY (process_id, value_type, label_id, timestamp);


CREATE TABLE memory_peaks
(
    process_id Int32,
    value_type Enum('active_size' = 1, 'active_count' = 2, 'total_size' = 3, 'total_count' = 4),
    peak_ts AggregateFunction(argMax, DateTime64(9), Int64)
)
ENGINE = AggregatingMergeTree
ORDER BY (process_id, value_type);

CREATE MATERIALIZED VIEW memory_peaks_mv TO memory_peaks AS
SELECT
    process_id,
    value_type,
    argMaxState(timestamp, total_value) AS peak_ts
FROM flamegraphs
WHERE order_id = 0 -- root node
GROUP BY (process_id, value_type);

CREATE TABLE IF NOT EXISTS stacktraces
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Stacktrace
    leaf_node_id UInt32,
    path Array(UInt32), -- label_id, from root to leaf node included
)
ENGINE = MergeTree()
ORDER BY (process_id, leaf_node_id);

CREATE TABLE IF NOT EXISTS graph_edges
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Graph edge
    label_id UInt32,
    node_id UInt32,
    parent_node_id UInt32,
)
ENGINE = MergeTree()
ORDER BY (process_id, parent_node_id, node_id);

CREATE TABLE IF NOT EXISTS localized_name
(
-- Process selectors
    process_id Int32, -- fkey on postgres processes

-- Labels data
    label_id UInt32,

    label String,
    location String,
    library String,
    frame_offset UInt64, -- for debugging purpose
)
ENGINE = MergeTree()
ORDER BY (process_id, label_id);
