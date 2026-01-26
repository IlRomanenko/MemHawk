CREATE SCHEMA IF NOT EXISTS profiling;

CREATE TABLE IF NOT EXISTS profiling.processes
(
    process_id SERIAL PRIMARY KEY,
    process_pid INTEGER NOT NULL,
    process_name TEXT NOT NULL,
    start_time BigInt NOT NULL,

    -- for debugging purposes
    created_at TIMESTAMPTZ DEFAULT NOW()    
);

CREATE INDEX IF NOT EXISTS idx_processes_pid_name_time 
ON profiling.processes (process_pid, process_name, start_time);

ALTER TABLE profiling.processes 
ADD CONSTRAINT complex_key_unique_constraint
UNIQUE (process_pid, process_name, start_time);

CREATE TABLE IF NOT EXISTS profiling.saved_states
(
    process_id INTEGER PRIMARY KEY REFERENCES profiling.processes(process_id) ON DELETE CASCADE,

    -- Binary data with TOAST optimization
    state BYTEA NOT NULL,
    original_size BigInt NOT NULL,
    checksum BYTEA NOT NULL,

    -- for debugging purposes
    created_at TIMESTAMPTZ DEFAULT NOW()
);

ALTER TABLE profiling.saved_states ALTER COLUMN state SET STORAGE EXTERNAL;  -- No in-line storage
