use clickhouse::Row;
use serde::{Deserialize, Serialize};
use time::OffsetDateTime;

#[derive(Row, Serialize, Deserialize)]
pub struct ProfileRow {
    // Process selectors
    pub process_id: i32,

    // Frame data
    #[serde(with = "clickhouse::serde::time::datetime64::nanos")]
    pub timestamp: OffsetDateTime, // DateTime64(9)
    pub node_id: u32,
    pub label_id: u32,

    // Actual memory profiling data
    pub self_active_size: i64,
    pub self_active_count: i64,
    pub self_overhead: i64,
    pub self_total_size: u64,
    pub self_total_count: u64,

    pub total_active_size: i64,
    pub total_active_count: i64,
    pub total_overhead: i64,
    pub total_total_size: u64,
    pub total_total_count: u64,
}

#[derive(Row, Serialize, Deserialize)]
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

#[derive(Row, Serialize, Deserialize)]
pub struct StacktraceRow {
    // Process selectors
    pub process_id: i32,

    // Stacktraces data
    pub label_id: u32,
    pub node_id: u32,
    pub path: Vec<u32>,
}

pub struct ClickhouseClient {
    client: clickhouse::Client,
}

impl ClickhouseClient {
    const PROFILE_TABLE: &str = "profiles";
    const LOCALIZED_NAME_TABLE: &str = "localized_name";
    const STACKTRACE_TABLE: &str = "stacktraces";

    pub fn new() -> Self {
        let client = clickhouse::Client::default()
            .with_url("http://localhost:8123")
            .with_user("admin")
            .with_password("admin")
            .with_database("profiling");
        ClickhouseClient { client }
    }

    pub async fn insert_localized_names(
        &self,
        rows: &[LocalizedNameRow],
    ) -> clickhouse::error::Result<()> {
        let mut inserter = self
            .client
            .insert::<LocalizedNameRow>(Self::LOCALIZED_NAME_TABLE)
            .await?;
        for row in rows {
            inserter.write(row).await?;
        }
        inserter.end().await?;
        Ok(())
    }

    pub async fn insert_profiles(&self, rows: Vec<ProfileRow>) -> clickhouse::error::Result<()> {
        let mut inserter = self
            .client
            .insert::<ProfileRow>(Self::PROFILE_TABLE)
            .await?;
        for row in rows {
            inserter.write(&row).await?;
        }
        inserter.end().await?;
        Ok(())
    }

    pub async fn insert_stacktraces(&self, rows: Vec<StacktraceRow>) -> clickhouse::error::Result<()> {
        let mut inserter = self
            .client
            .insert::<StacktraceRow>(Self::STACKTRACE_TABLE)
            .await?;
        for row in rows {
            inserter.write(&row).await?;
        }
        inserter.end().await?;
        Ok(())
    }
}

impl Default for ClickhouseClient {
    fn default() -> Self {
        Self::new()
    }
}
