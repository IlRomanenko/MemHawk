use std::{
    fs::File,
    io::{BufReader, BufWriter, Read, Write},
};

pub struct ProtosRepository {}

impl ProtosRepository {
    pub fn read_messages<T>(filename: &str) -> anyhow::Result<Vec<T>>
    where
        T: prost::Message + Default,
    {
        let file = File::open(filename)?;
        let mut reader = BufReader::new(file);

        let mut buffer = Vec::new();
        reader.read_to_end(&mut buffer)?;

        let mut result = Vec::new();

        let mut span = bytes::Bytes::from(buffer);
        while let Ok(elem) = T::decode_length_delimited(&mut span) {
            result.push(elem);
        }
        Ok(result)
    }

    pub fn write_messages<T>(filename: &str, msgs: &Vec<T>) -> anyhow::Result<()>
    where
        T: prost::Message + Default,
    {
        let file = File::create(filename)?;
        let mut writer = BufWriter::new(file);

        for elem in msgs {
            let buf = elem.encode_length_delimited_to_vec();
            writer.write_all(&buf)?;
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::protos;

    use super::*;

    fn new_snapshot() -> protos::Snapshot {
        protos::Snapshot {
            timestamp: 10,
            changed: vec![protos::TracedAllocSummary {
                trace_id: 10,
                actual: Some(protos::AllocSummary {
                    size: 10,
                    active: 11,
                    overhead: 12,
                    total_count: 34545,
                    total_bytes: 231412414,
                }),
            }],
            ptr_ids: Vec::new(),
            stacktraces: Vec::new(),
            loaded_so: Vec::new(),
            process: Some(protos::ProcessInfo {
                process_short_name: "some_process".to_owned(),
                process_full_path: "/usr/bin/some_process".to_owned(),
                pid: 12,
                start_timestamp: 0,
            }),
            total: Some(protos::AllocSummary {
                size: 10,
                active: 11,
                overhead: 12,
                total_count: 34545,
                total_bytes: 231412414,
            }),
        }
    }

    #[test]
    pub fn test_rw_messages() -> anyhow::Result<()> {
        let msgs = vec![new_snapshot(), new_snapshot()];
        ProtosRepository::write_messages::<protos::Snapshot>("/tmp/test", &msgs)?;

        let result = ProtosRepository::read_messages::<protos::Snapshot>("/tmp/test")?;

        assert_eq!(msgs.len(), result.len());
        assert_eq!(msgs, result);
        Ok(())
    }
}
