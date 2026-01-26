use std::time::Duration;
use tokio::fs::File;
use tokio::io::{AsyncReadExt, BufReader};

async fn read_retry(reader: &mut BufReader<File>, buf: &mut [u8]) -> anyhow::Result<()> {
    let mut completed = 0;
    while completed < buf.len() {
        let n = reader.read(&mut buf[completed..]).await?;
        if n == 0 {
            tokio::time::sleep(Duration::from_millis(100)).await;
            continue;
        }
        completed += n;
    }
    Ok(())
}

async fn read_exact(reader: &mut BufReader<File>, buf: &mut [u8]) -> anyhow::Result<()> {
    reader.read_exact(buf).await?;
    Ok(())
}

pub struct ProtoReader {
    reader: BufReader<File>,
    buffer: Vec<u8>,
}

impl ProtoReader {
    const TAG_SIZE: usize = size_of::<u64>();

    pub async fn new(filename: &str) -> anyhow::Result<Self> {
        let file = File::open(filename).await?;
        let reader = BufReader::new(file);
        let result = Self {
            reader,
            buffer: Vec::default(),
        };
        Ok(result)
    }

    pub async fn read_message<T>(&mut self) -> anyhow::Result<T>
    where
        T: prost::Message + Default,
    {
        let mut msg_buf = [0; Self::TAG_SIZE];
        read_exact(&mut self.reader, &mut msg_buf).await?;
        let message_size = u64::from_be_bytes(msg_buf);

        self.buffer.resize(message_size as usize, 0);
        read_exact(&mut self.reader, &mut self.buffer[..]).await?;
        let decompressed = zstd::decode_all(&self.buffer[..])?;
        let msg = T::decode(&decompressed[..])?;
        Ok(msg)
    }

    pub async fn read_message_appendable<T>(&mut self) -> anyhow::Result<T>
    where
        T: prost::Message + Default,
    {
        let mut msg_buf = [0; Self::TAG_SIZE];
        read_retry(&mut self.reader, &mut msg_buf).await?;
        let message_size = u64::from_be_bytes(msg_buf);

        self.buffer.resize(message_size as usize, 0);
        read_retry(&mut self.reader, &mut self.buffer[..]).await?;
        let decompressed = zstd::decode_all(&self.buffer[..])?;
        let msg = T::decode(&decompressed[..])?;
        Ok(msg)
    }
}
