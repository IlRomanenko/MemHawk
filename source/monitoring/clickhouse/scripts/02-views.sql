-- Define parameterized views

use profiling;

-- Params:
-- process_id:Int32, value_type:String,
-- from_time:DateTime64(9), to_time:DateTime64(9),
-- diff_sign:Int32, pass_all:Boolean
CREATE OR REPLACE VIEW flamegraph_view AS 
SELECT 
    label || ':' || library || ':' || location as label,
    level,
    self_value as self,
    total_value as value
FROM
    executable(
        'flamegraph',
        TabSeparated,
        'order_id UInt32, label_id UInt32, level UInt32, total_value Int64, self_value Int64',
        (
            SELECT
                value,
                path
            FROM (
                SELECT 
                    leaf_node_id,
                    (right.value - ifNull(left.value, 0)) * {diff_sign:Int32} as value
                FROM (
                    SELECT
                        leaf_node_id,
                        argMax(value, timestamp) as value
                    FROM
                        raw_data
                    WHERE
                        process_id == {process_id:Int32}
                        AND timestamp > {from_time:DateTime64(9)}
                        AND timestamp <= {to_time:DateTime64(9)}
                        AND value_type == {value_type:String}
                    GROUP BY(leaf_node_id)
                ) AS right
                LEFT JOIN (
                    SELECT
                        leaf_node_id,
                        argMax(value, timestamp) as value
                    FROM
                        raw_data
                    WHERE
                        process_id == {process_id:Int32}
                        AND timestamp <= {from_time:DateTime64(9)}
                        AND value_type == {value_type:String}
                    GROUP BY(leaf_node_id)
                    HAVING value != 0 -- eliminate empty frames
                ) as left
                USING (leaf_node_id)
                WHERE 
                    if({pass_all:Boolean}, 1 == 1, value > 0)
                    AND value != 0 -- eliminate empty frames
                ORDER BY (leaf_node_id)
            ) as a
            INNER JOIN (
                SELECT
                    leaf_node_id,
                    path
                FROM
                    stacktraces
                WHERE 
                    process_id == {process_id:Int32}
                ORDER BY (leaf_node_id)
            ) as b
            USING (leaf_node_id)
        )
    ) as a
LEFT JOIN (
    SELECT
        label,
        library,
        location,
        label_id
    FROM
        localized_name
    WHERE
        process_id == {process_id:Int32}
) as b
USING (label_id)
ORDER BY order_id ASC;

-- Params:
-- process_id:Int32, value_type:String,
-- from_time:DateTime64(9), to_time:DateTime64(9)
-- interval_ms:UInt32, labels:Array(UInt32)
CREATE OR REPLACE VIEW timeseries_view AS
WITH (
    -- get grid from root node
    SELECT
        groupArray(time) as grid
    FROM (
        SELECT
            toStartOfInterval(timestamp, INTERVAL {interval_ms:UInt32} millisecond) as time
        FROM
            profiling.timeseries
        WHERE
            timestamp >= {from_time:DateTime64(9)}
            AND timestamp <= {to_time:DateTime64(9)}
            AND process_id == {process_id:Int32}
            AND label_id == 0
            AND value_type == {value_type:String}
        GROUP BY (time)
    )
    GROUP BY ALL
) as grid
SELECT
    label || ':' || library || ':' || location as label,
    resampled.1 as time,
    resampled.2 as value
FROM (
    SELECT
        label_id,
        arrayJoin(
            UserGridResampling(
                result_grid,
                arrayMap(x -> x.1, series),
                arrayMap(x -> x.2, series)
            )
        ) AS resampled
    FROM (
        SELECT
            label_id,
            groupArray((time, value)) as series,
            -- join root grid with grid for each label
            -- (can have zero values in range and one value from earlier times)
            UserUniqueConcat(grid, groupArray(time)) as result_grid
        FROM (
            -- select values within time interval
            SELECT
                label_id,
                max(total_value) as value,
                toStartOfInterval(timestamp, INTERVAL {interval_ms:UInt32} millisecond) as time
            FROM
                profiling.timeseries
            WHERE
                timestamp >= {from_time:DateTime64(9)}
                AND timestamp <= {to_time:DateTime64(9)}
                AND process_id == {process_id:Int32}
                AND label_id IN {labels:Array(UInt32)}
                AND value_type == {value_type:String}
            GROUP BY (time, label_id)

            -- add last known value before time interval
            UNION ALL
            SELECT
                label_id,
                argMax(total_value, timestamp) as value,
                toStartOfInterval(max(timestamp), INTERVAL {interval_ms:UInt32} millisecond) as time
            FROM
                profiling.timeseries
            WHERE
                timestamp < {from_time:DateTime64(9)}
                AND process_id == {process_id:Int32}
                AND label_id IN {labels:Array(UInt32)}
                AND value_type == {value_type:String}
            GROUP BY (label_id)
        )
        GROUP BY (label_id)
    )
) as a
LEFT JOIN (
    SELECT
        label,
        library,
        location,
        label_id
    FROM
        localized_name
    WHERE
        process_id == {process_id:Int32}
        AND label_id IN {labels:Array(UInt32)}
) as b
USING (label_id)
ORDER BY label, time ASC;
