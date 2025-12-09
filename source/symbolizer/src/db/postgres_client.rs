use sha2::Digest;
use sqlx::postgres::PgConnectOptions;
use sqlx::{FromRow, Row};

use crate::protos;

#[derive(FromRow)]
pub struct SavedState {
    state: Vec<u8>,
    original_size: i64,
    checksum: Vec<u8>,
}

impl SavedState {
    pub fn compress(state: Vec<u8>) -> anyhow::Result<Self> {
        let original_size = state.len();
        let compressed = zstd::bulk::compress(&state, 0)?;
        let checksum = sha2::Sha256::digest(&compressed);
        Ok(Self {
            state: compressed,
            original_size: original_size as i64,
            checksum: checksum.to_vec(),
        })
    }

    pub fn decompress(self) -> anyhow::Result<Vec<u8>> {
        if !self.is_valid() {
            anyhow::bail!("Incorrect checksum, can't decompress state");
        }
        let original = zstd::bulk::decompress(&self.state, self.original_size as usize)?;
        Ok(original)
    }

    pub fn is_valid(&self) -> bool {
        let checksum = sha2::Sha256::digest(&self.state);
        return checksum.to_vec().eq(&self.checksum);
    }
}

pub struct PostgresClient {
    pool: sqlx::postgres::PgPool,
}

impl PostgresClient {
    pub async fn new() -> anyhow::Result<Self> {
        let options = PgConnectOptions::new()
            .username("admin")
            .password("admin")
            .port(5432);
        let pool = sqlx::postgres::PgPool::connect_with(options).await?;
        Ok(PostgresClient { pool })
    }

    pub async fn get_process_id(&self, process_info: &protos::ProcessInfo) -> sqlx::Result<i32> {
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
        .bind(process_info.process_short_name.clone())
        .bind(process_info.pid as i32)
        .bind(process_info.start_timestamp as i64)
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

    pub async fn save_process_state(&self, process_id: i32, state: SavedState) -> anyhow::Result<()> {
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
}
