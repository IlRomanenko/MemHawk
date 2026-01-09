use anyhow::Result;
use tokio::sync::mpsc;

use clickhouse::{Client, RowOwned, RowWrite};
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
    client: Client,
    table: &'static str,
    rx: mpsc::Receiver<Vec<T>>,
}

impl<T> MsgsSender<T>
where
    T: RowOwned + RowWrite + Send + Sync,
{
    pub fn new(client: Client, table: &'static str, rx: mpsc::Receiver<Vec<T>>) -> Self {
        Self { client, table, rx }
    }

    pub async fn start(&mut self) {
        while let Some(msgs) = self.rx.recv().await {
            if let Err(e) = self.send_msg(msgs).await {
                log::error!("Failed to insert into {}, error: {}", self.table, e);
            }
        }
        log::info!("Stopped sender for table {}", self.table);
    }

    async fn send_msg(&self, msgs: Vec<T>) -> anyhow::Result<()> {
        if msgs.is_empty() {
            return Ok(());
        }
        let mut inserter = self.client.insert::<T>(self.table).await?;
        for msg in msgs {
            inserter.write(&msg).await?;
        }
        inserter.end().await?;
        Ok(())
    }
}
