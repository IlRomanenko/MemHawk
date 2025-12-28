use clap::Parser;
use std::env::args;
use tokio_util::task::TaskTracker;

use symbolizer::{
    postgres::client::PostgresClient,
    processor::{self, Processor},
    proto::{
        reader::ProtoReader,
        schema::{ElfInfo, ProcessInfo, Snapshot, elf_info},
    },
    symbolizer::Symbolizer,
};

#[derive(Parser)]
#[command(version, about)]
struct Args {
    #[arg(short, long)]
    filename: String,
}

async fn process(task_tracker: &TaskTracker) -> anyhow::Result<()> {
    let args = Args::parse();
    let filename = args.filename;

    log::info!("Starting");

    let mut reader = ProtoReader::new(&filename).await?;
    let process_info = reader.read_message::<ProcessInfo>().await?;

    let postgres_client = PostgresClient::new().await?;
    let process_id = postgres_client
        .get_process_id(
            &process_info.process_short_name,
            process_info.pid as i32,
            process_info.start_timestamp as i64,
        )
        .await?;

    let state = postgres_client.read_process_state(process_id).await?;

    log::info!("Read saved state");

    let mut processor = match state {
        Some(state) => {
            let encoded = state.decompress()?;
            let processor_state = bitcode::decode::<processor::RestorableState>(&encoded)?;
            Processor::restore(task_tracker, processor_state).await?
        }
        None => Processor::new(task_tracker, process_id),
    };
    log::info!("Restored state");

    while let Ok(message) = reader.read_message::<Snapshot>().await {
        let _ = processor.process(&message).await?;
    }

    log::info!("Completed processing");

    processor.stats().await;

    // let processor_state = processor.save().await;
    // let encoded = bitcode::encode(&processor_state);
    // let state = SavedState::compress(encoded)?;

    // log::info!("Encoded state");

    // postgres_client
    //     .save_process_state(process_id, state)
    //     .await?;

    // log::info!("Saved state");
    Ok(())
}

async fn test_symbolizer() {
    let args = args().collect::<Vec<_>>();
    let filename = args[1].clone();

    let probe = u32::from_str_radix(&args[2].strip_prefix("0x").unwrap(), 16).unwrap();

    let mut symbolizer = Symbolizer::new();
    let test_binary = vec![ElfInfo {
        filename,
        addr: 0,
        segments: vec![elf_info::Segment { addr: 0, size: 0 }],
    }];
    symbolizer.update_symbols(&test_binary).await;

    let frames = symbolizer.lookup_symbol(probe.into()).await.unwrap();
    for frame in frames.inlined.iter() {
        println!("{:?}", frame);
    }
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    env_logger::init_from_env(env_logger::Env::default().default_filter_or("info"));
    let tracker = TaskTracker::new();
    process(&tracker).await?;
    tracker.close();
    tracker.wait().await;
    Ok(())
}
