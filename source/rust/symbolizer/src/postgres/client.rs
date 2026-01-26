use crate::postgres::state::SavedState;
use sqlx::Row;
use sqlx::postgres::PgConnectOptions;

pub struct PostgresClient {
    pool: sqlx::postgres::PgPool,
}

impl PostgresClient {
    pub async fn new(user: &str, password: &str, port: u16) -> anyhow::Result<Self> {
        let options = PgConnectOptions::new()
            .username(user)
            .password(password)
            .port(port);
        let pool = sqlx::postgres::PgPool::connect_with(options).await?;
        Ok(PostgresClient { pool })
    }

    pub async fn get_process_id(
        &self,
        process_name: &str,
        process_pid: i32,
        start_timestamp: i64,
    ) -> sqlx::Result<i32> {
        let row = sqlx::query(
            r#"
INSERT INTO profiling.processes (process_name, process_pid, start_time)
VALUES ($1, $2, $3)
ON CONFLICT (process_name, process_pid, start_time)
DO UPDATE SET 
    process_name = EXCLUDED.process_name  -- No-op update
RETURNING process_id
            "#,
        )
        .bind(process_name)
        .bind(process_pid)
        .bind(start_timestamp)
        .fetch_one(&self.pool)
        .await?;
        row.try_get::<i32, _>("process_id")
    }

    pub async fn read_process_state(&self, process_id: i32) -> sqlx::Result<Option<SavedState>> {
        sqlx::query_as::<_, SavedState>(
            r#"
SELECT
    state, original_size, checksum
FROM
    profiling.saved_states
WHERE
    process_id = $1
            "#,
        )
        .bind(process_id)
        .fetch_optional(&self.pool)
        .await
    }

    pub async fn save_process_state(
        &self,
        process_id: i32,
        state: SavedState,
    ) -> anyhow::Result<()> {
        sqlx::query(
            r#"
INSERT INTO profiling.saved_states (process_id, state, original_size, checksum)
VALUES ($1, $2, $3, $4)
ON CONFLICT (process_id)
DO UPDATE SET 
    state = EXCLUDED.state,
    original_size = EXCLUDED.original_size,
    checksum = EXCLUDED.checksum
            "#,
        )
        .bind(process_id)
        .bind(state.state)
        .bind(state.original_size)
        .bind(state.checksum)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    pub async fn drop_process_state(&self, process_id: i32) -> anyhow::Result<()> {
        sqlx::query(
            r#"
DELETE FROM profiling.saved_states WHERE process_id = $1
            "#,
        )
        .bind(process_id)
        .execute(&self.pool)
        .await?;
        Ok(())
    }
}
