use anyhow::bail;
use clap::Parser;
use std::env::args;

use symbolizer::{
    db::postgres_client::{PostgresClient, SavedState},
    processor::{self, Processor},
    protos::{self, ElfInfo, elf_info},
    repository,
    symbolizer::Symbolizer,
};

#[derive(Parser)]
#[command(version, about)]
struct Args {
    #[arg(short, long)]
    filename: String,
}

async fn process() -> anyhow::Result<()> {
    let args = Args::parse();
    let filename = args.filename;

    log::info!("Starting");

    let messages = repository::ProtosRepository::read_messages::<protos::Snapshot>(&filename)?;
    let first_message = match messages.first() {
        Some(msg) => msg,
        None => return Ok(()),
    };

    let process_info = match &first_message.process {
        Some(process_info) => process_info,
        None => {
            bail!("Incorrect first message")
        }
    };

    let postgres_client = PostgresClient::new().await?;
    let process_id = postgres_client.get_process_id(&process_info).await?;

    let state = postgres_client.read_process_state(process_id).await?;

    log::info!("Read saved state");

    let mut processor = match state {
        Some(state) => {
            let encoded = state.decompress()?;
            let processor_state = bitcode::decode::<processor::RestorableState>(&encoded)?;
            Processor::restore(processor_state).await?
        }
        None => Processor::new(process_id),
    };
    log::info!("Restored state");

    for message in messages {
        let _ = processor.process(&message).await?;
    }

    log::info!("Completed processing");

    processor.stats().await;

    let processor_state = processor.save().await;
    let encoded = bitcode::encode(&processor_state);
    let state = SavedState::compress(encoded)?;

    log::info!("Encoded state");

    postgres_client
        .save_process_state(process_id, state)
        .await?;

    log::info!("Saved state");
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
    env_logger::init();
    process().await
}
