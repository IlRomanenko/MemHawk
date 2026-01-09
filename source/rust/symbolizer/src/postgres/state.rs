use sha2::Digest;
use sqlx::FromRow;

#[derive(FromRow)]
pub struct SavedState {
    pub state: Vec<u8>,
    pub original_size: i64,
    pub checksum: Vec<u8>,
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
