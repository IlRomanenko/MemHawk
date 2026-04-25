use anyhow::Result;
use tokio::sync::mpsc;

use clickhouse::{Client, RowOwned, RowWrite, inserter::Inserter};
use tokio_util::task::TaskTracker;

pub struct TableWorker<T> {
    tx: mpsc::Sender<Vec<T>>,
}

impl<T> TableWorker<T>
where
    T: RowOwned + RowWrite + Send + Sync,
{
    pub fn new(
        task_tracker: &TaskTracker,
        client: Client,
        table: &'static str,
        capacity: usize,
    ) -> Self {
        let (tx, rx) = mpsc::channel::<Vec<T>>(capacity);

        let mut sender = MsgsSender::new(client, table, rx);
        // spawn sender loop
        task_tracker.spawn(async move {
            sender.start().await;
        });

        Self { tx }
    }

    pub async fn send(&self, rows: Vec<T>) -> Result<()> {
        self.tx.send(rows).await?;
        Ok(())
    }
}

struct MsgsSender<T> {
    inserter: Inserter<T>,
    table: &'static str,
    rx: mpsc::Receiver<Vec<T>>,
}

impl<T> MsgsSender<T>
where
    T: RowOwned + RowWrite + Send + Sync,
{
    pub fn new(client: Client, table: &'static str, rx: mpsc::Receiver<Vec<T>>) -> Self {
        let inserter = client
            .inserter::<T>(table)
            .with_max_bytes(50_000_000)
            .with_max_rows(750_000)
            .with_period(Some(std::time::Duration::from_secs(1)));
        Self {
            inserter,
            table,
            rx,
        }
    }

    pub async fn start(&mut self) {
        while let Some(msgs) = self.rx.recv().await {
            if let Err(e) = self.send_msg(msgs).await {
                log::error!("Failed to insert into {}, error: {}", self.table, e);
            }
        }
        if let Err(e) = self.inserter.force_commit().await {
            log::error!("Failed to insert into {}, error: {}", self.table, e);
        }
        log::info!("Stopped sender for table {}", self.table);
    }

    async fn send_msg(&mut self, msgs: Vec<T>) -> anyhow::Result<()> {
        if msgs.is_empty() {
            return Ok(());
        }
        for msg in msgs {
            self.inserter.write(&msg).await?;
        }
        self.inserter.commit().await?;
        Ok(())
    }
}
