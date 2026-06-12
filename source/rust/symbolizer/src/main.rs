use clap::{Args, Parser, Subcommand};
use clap_num::maybe_hex;
use memhawk_core::symbolizer::Symbolizer;
use symbolizer::clickhouse::client::ClickhouseClient;
use symbolizer::postgres::client::PostgresClient;
use symbolizer::postgres::state::SavedState;
use symbolizer::processor::{self, Processor};
use symbolizer::proto::reader::ProtoReader;
use tokio_util::task::TaskTracker;

use memhawk_core::proto::schema::{ElfInfo, ProcessInfo, Snapshot, elf_info};

#[derive(Debug, Parser)]
#[command(version, about)]
#[command(propagate_version = true)]
pub struct AppArguments {
    #[clap(subcommand)]
    command: Commands,
}

#[derive(Debug, Clone, Args)]
struct ProcessorArgs {
    #[arg(short, long)]
    filename: String,
    #[arg(short, long)]
    sysroot: Option<String>,
    #[arg(long)]
    force: bool,
    #[arg(long)]
    watch: bool,
    #[arg(long)]
    with_location: bool,
    #[command(flatten)]
    clickhouse: ClickhouseArgs,
    #[command(flatten)]
    postgres: PostgresArgs,
}

#[derive(Debug, Clone, Args)]
struct ClickhouseArgs {
    #[arg(long, default_value = "http://localhost:8123")]
    clickhouse_url: String,
    #[arg(long, default_value = "admin")]
    clickhouse_user: String,
    #[arg(long, default_value = "admin")]
    clickhouse_password: String,
}

#[derive(Debug, Clone, Args)]
struct PostgresArgs {
    #[arg(long, default_value_t = 5432)]
    postgres_port: u16,
    #[arg(long, default_value = "admin")]
    postgres_user: String,
    #[arg(long, default_value = "admin")]
    postgres_password: String,
}

#[derive(Debug, Clone, Args)]
struct Addr2LineArgs {
    #[arg(short, long)]
    filename: String,
    #[arg(short, long, value_parser=maybe_hex::<u64>)]
    addr: u64,
}

#[derive(Debug, Clone, Subcommand)]
enum Commands {
    Processor(ProcessorArgs),
    Addr2line(Addr2LineArgs),
}

async fn create_processor(
    args: &ProcessorArgs,
    task_tracker: &TaskTracker,
    postgres_client: &PostgresClient,
    process_id: i32,
) -> anyhow::Result<processor::Processor> {
    let click = ClickhouseClient::new(
        task_tracker,
        &args.clickhouse.clickhouse_url,
        &args.clickhouse.clickhouse_user,
        &args.clickhouse.clickhouse_password,
    );
    if args.force {
        log::info!("Force mode. Dropping saved state if there is any");
        postgres_client.drop_process_state(process_id).await?;
        return Processor::new(click, process_id, args.sysroot.clone(), args.with_location).await;
    }
    let state = postgres_client.read_process_state(process_id).await?;
    let processor = match state {
        Some(state) => {
            log::info!("Restoring state");
            let encoded = state.decompress()?;
            let processor_state = bitcode::decode::<processor::RestorableState>(&encoded)?;
            Processor::restore(click, processor_state, args.sysroot.clone()).await?
        }
        None => {
            log::info!("Creating new processor");
            Processor::new(click, process_id, args.sysroot.clone(), args.with_location).await?
        }
    };
    Ok(processor)
}

async fn process(args: ProcessorArgs, task_tracker: &TaskTracker) -> anyhow::Result<()> {
    let filename = args.filename.clone();

    log::info!("Starting");

    let mut reader = ProtoReader::new(&filename).await?;
    let process_info = reader.read_message::<ProcessInfo>().await?;

    let postgres_client = PostgresClient::new(
        &args.postgres.postgres_user,
        &args.postgres.postgres_password,
        args.postgres.postgres_port,
    )
    .await?;
    let process_id = postgres_client
        .get_process_id(
            &process_info.process_short_name,
            process_info.pid as i32,
            process_info.start_timestamp as i64,
        )
        .await?;

    log::info!("Got process_id: {}", process_id);

    let mut processor = create_processor(&args, task_tracker, &postgres_client, process_id).await?;

    if args.watch {
        loop {
            match tokio::time::timeout(
                tokio::time::Duration::from_secs(2),
                reader.read_message_appendable::<Snapshot>(),
            )
            .await
            {
                Ok(read_result) => match read_result {
                    Ok(message) => {
                        processor.process(&message).await?;
                    }
                    Err(e) => {
                        log::error!("Error during reading: {:?}", e);
                        break;
                    }
                },
                Err(_) => {
                    log::info!("Waiting timeout expired");
                    break;
                }
            }
        }
    } else {
        while let Ok(message) = reader.read_message::<Snapshot>().await {
            let _ = processor.process(&message).await?;
        }
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

async fn addr2line(args: Addr2LineArgs) -> anyhow::Result<()> {
    let mut symbolizer = Symbolizer::new(true);
    let binary_stub = vec![ElfInfo {
        filename: args.filename,
        addr: 0,
        segments: vec![elf_info::Segment { addr: 0, size: 0 }],
    }];
    symbolizer.update_symbols(&binary_stub).await;

    let frames = symbolizer.lookup_symbol(args.addr).await?;
    println!("{:?}", frames);
    Ok(())
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    env_logger::init_from_env(env_logger::Env::default().default_filter_or("info"));
    let tracker = TaskTracker::new();
    let args = AppArguments::parse();
    match args.command {
        Commands::Processor(processor_args) => process(processor_args, &tracker).await,
        Commands::Addr2line(addr2_line_args) => addr2line(addr2_line_args).await,
    }?;
    tracker.close();
    tracker.wait().await;
    Ok(())
}
