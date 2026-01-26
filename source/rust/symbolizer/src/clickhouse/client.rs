use tokio_util::task::TaskTracker;

use crate::clickhouse::{schema::*, table_worker::TableWorker};

pub struct ClickhouseClient {
    client: clickhouse::Client,
    raw_data_worker: TableWorker<RawDataRow>,
    flamegraph_worker: TableWorker<FlamegraphRow>,
    timeseries_worker: TableWorker<TimeseriesRow>,
    localized_name_worker: TableWorker<LocalizedNameRow>,
    graph_edges_worker: TableWorker<GraphEdgeRow>,
    stacktraces_worker: TableWorker<StacktraceRow>,
}

impl ClickhouseClient {
    pub fn new(task_tracker: &TaskTracker, url: &str, user: &str, password: &str) -> Self {
        let client = clickhouse::Client::default()
            .with_url(url)
            .with_user(user)
            .with_password(password)
            .with_database("profiling");

        ClickhouseClient {
            client: client.clone(),
            raw_data_worker: TableWorker::new(task_tracker, client.clone(), "raw_data", 2),
            flamegraph_worker: TableWorker::new(task_tracker, client.clone(), "flamegraphs", 2),
            timeseries_worker: TableWorker::new(task_tracker, client.clone(), "timeseries", 2),
            localized_name_worker: TableWorker::new(
                task_tracker,
                client.clone(),
                "localized_name",
                2,
            ),
            graph_edges_worker: TableWorker::new(task_tracker, client.clone(), "graph_edges", 2),
            stacktraces_worker: TableWorker::new(task_tracker, client.clone(), "stacktraces", 2),
        }
    }

    pub async fn clear(&self, process_id: i32) -> anyhow::Result<()> {
        let tables = vec![
            "raw_data", "flamegraphs", "timeseries", "localized_name", "graph_edges", "stacktraces"
        ];
        for table in tables {
            let query = format!("DELETE FROM profiling.{} WHERE process_id == ?", table);
            self.client
                .query(&query)
                .bind(process_id)
                .execute()
                .await?;
        }
        Ok(())
    }

    pub async fn send(&self, update: UpdateData) -> anyhow::Result<()> {
        self.raw_data_worker.send(update.raw_data).await?;
        self.flamegraph_worker.send(update.flamegraphs).await?;
        self.timeseries_worker.send(update.timeseries).await?;
        self.localized_name_worker
            .send(update.localized_names)
            .await?;
        self.graph_edges_worker.send(update.graph_edges).await?;
        self.stacktraces_worker.send(update.stacktraces).await?;
        Ok(())
    }
}
