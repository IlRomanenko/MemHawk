use tokio::fs::File;
use tokio::io::{AsyncReadExt, BufReader};

pub struct ProtoReader {
    reader: BufReader<File>,
    buffer: Vec<u8>,
}

impl ProtoReader {
    pub async fn new(filename: &str) -> anyhow::Result<Self> {
        let file = File::open(filename).await?;
        let reader = BufReader::new(file);
        let result = Self {
            reader,
            buffer: Vec::new(),
        };
        Ok(result)
    }

    pub async fn read_message<T>(&mut self) -> anyhow::Result<T>
    where
        T: prost::Message + Default,
    {
        let message_size = self.reader.read_u64().await?;
        self.buffer.resize(message_size as usize, 0);
        self.reader.read_exact(&mut self.buffer).await?;
        let decompressed = zstd::decode_all(&self.buffer[..])?;
        let msg = T::decode(&decompressed[..])?;
        Ok(msg)
    }
}
