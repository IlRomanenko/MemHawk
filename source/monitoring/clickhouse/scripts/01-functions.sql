-- Define helpers functions

use profiling;

CREATE OR REPLACE FUNCTION UserUniqueConcat AS (grid, grid_2) -> 
arraySort(
    arrayReduce(
        'groupUniqArray',
        arraySort(arrayConcat(grid, grid_2))
    )
);

CREATE OR REPLACE FUNCTION UserGridResampling AS (grid, series_ts, series_value) ->
arrayZip(
    grid,
    arrayFill(
        x -> isNotNull(x),
        arrayMap(
            ts -> if(
                has(series_ts, ts),
                toNullable(arrayElement(series_value, indexOf(series_ts, ts))),
                NULL
            ),
            grid
        )
    )
);
