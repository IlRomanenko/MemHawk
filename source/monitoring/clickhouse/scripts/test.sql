WITH (
    SELECT
        timestamp,
        SUM(total_active_size) as value
    FROM
        profiles
    WHERE
        node_id == 0
    GROUP BY (timestamp)
    ORDER BY value DESC, timestamp DESC
    LIMIT 1
) as max_rss
SELECT
    b.label ||'::'|| b.location as label,
    length(path) - 1 as level,
    total as value,
    self
FROM (
    SELECT
        a.label_id,
        b.path,
        a.total,
        a.self
    FROM (
        SELECT
            any(label_id) as label_id,
            node_id,
            argMax(total_active_size, timestamp) as total,
            argMax(self_active_size, timestamp) as self
        FROM
            profiles
        WHERE
            timestamp <= max_rss.timestamp
        GROUP BY (node_id)
        HAVING total > 0
        ORDER BY 
            total DESC,
            node_id ASC
        LIMIT 200
    ) as a
    INNER JOIN
        stacktraces as b
    ON (a.node_id == b.node_id)
) as a
INNER JOIN
    localized_name as b
ON (a.label_id == b.label_id)
ORDER BY (path)


SELECT
    timestamp,
    label as metric,
    sum(active_size) as value
FROM
    profiles
WHERE
    multiSearchAny (label, [$Symbols])
    -- AND timestamp >= $__timeFrom() AND timestamp < $__timeTo()
GROUP BY
    (label, timestamp)
ORDER BY
    (timestamp);





SELECT 
    st.node_id,
    arrayMap(
        x -> (
            SELECT label_id as y
            FROM profiling.stacktraces as ln
            WHERE ln.node_id = x
            LIMIT 1
        ),
        st.path
    ) as trace
FROM (
    SELECT
        *
    FROM
        profiling.stacktraces
    LIMIT 1
) as st;