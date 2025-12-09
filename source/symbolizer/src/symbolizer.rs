use anyhow::Result;
use bitcode::{Decode, Encode};
use rustc_hash::FxHashSet;
use std::sync::Arc;

use crate::protos;

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub struct InlinedFrame {
    pub name: String,
    pub location: String,
}

#[derive(Debug, Clone)]
pub struct SymbolizedFrame {
    pub symbol_name: String,
    pub library_name: Arc<String>,
    pub inlined: Vec<InlinedFrame>,
    pub offset: u64,
}

#[derive(PartialEq, Eq, PartialOrd, Ord, Clone, Copy)]
struct Interval {
    start: u64,
    size: u64,
}

struct LoadedBinary {
    filename: Arc<String>,
    interval: Interval,
    first_segment_offset: u64,
    symbol_map: Option<wholesym::SymbolMap>,
}

#[derive(Encode, Decode)]
struct RestorableBinary {
    filename: String,
    binary_addr: u64,
    first_segment_offset: u64,
}

#[derive(Encode, Decode)]
pub struct RestorableState {
    binaries: Vec<RestorableBinary>,
}

pub struct Symbolizer {
    symbol_manager: wholesym::SymbolManager,
    symbol_maps: Vec<LoadedBinary>,
}

impl Symbolizer {
    pub fn new() -> Self {
        let config = wholesym::SymbolManagerConfig::new();
        Self {
            symbol_manager: wholesym::SymbolManager::with_config(config),
            symbol_maps: Vec::new(),
        }
    }

    pub fn save(&self) -> RestorableState {
        RestorableState {
            binaries: self
                .symbol_maps
                .iter()
                .map(|x| RestorableBinary {
                    filename: x.filename.as_ref().clone(),
                    binary_addr: x.interval.start,
                    first_segment_offset: x.first_segment_offset,
                })
                .collect(),
        }
    }

    pub async fn restore(state: RestorableState) -> Self {
        let mut symbolizer = Self::new();
        for binary in state.binaries {
            symbolizer
                .load_binary(
                    binary.filename,
                    binary.binary_addr,
                    binary.first_segment_offset,
                )
                .await;
        }
        symbolizer
    }

    pub async fn lookup_symbol(&self, addr: u64) -> Result<SymbolizedFrame> {
        let index = self
            .symbol_maps
            .partition_point(|binary| binary.interval.start <= addr);
        let slice = &self.symbol_maps[..index];
        let binary = match slice.last() {
            Some(value) => value,
            None => anyhow::bail!("No segments containing vaddr: 0x{:x}", addr),
        };
        let offset = (addr - binary.first_segment_offset - binary.interval.start) as u32;

        let symbol_map = match &binary.symbol_map {
            Some(symbol_map) => symbol_map,
            None => anyhow::bail!("No symbol map for file: {}", &binary.filename),
        };

        let lookup_result = match symbol_map.lookup_sync(wholesym::LookupAddress::Relative(offset))
        {
            Some(addr_info) => addr_info,
            None => anyhow::bail!(
                "No segments containing vaddr: 0x{:x}, lib: {}, offset: 0x{:x}",
                addr,
                &binary.filename,
                offset
            ),
        };

        let mut inlined = Vec::new();
        if let Some(frames) = lookup_result.frames {
            match frames {
                wholesym::FramesLookupResult::Available(frame_debug_infos) => {
                    for info in frame_debug_infos.into_iter().rev() {
                        let function_name = match info.function {
                            Some(value) => value,
                            None => continue,
                        };
                        let file_path = match info.file_path {
                            Some(ref path) => path.raw_path().to_owned(),
                            None => "?".to_owned(),
                        };
                        let line_number = match info.line_number {
                            Some(line) => line.to_string(),
                            None => "?".to_owned(),
                        };
                        let location = format!("{}:{}", file_path, line_number);
                        inlined.push(InlinedFrame {
                            name: function_name,
                            location: location,
                        })
                    }
                }
                wholesym::FramesLookupResult::External(_) => {
                    log::warn!(
                        "Can't work with external frames, skipping vaddr: 0x{:x}, lib: {}, offset: 0x{:x}",
                        addr,
                        &binary.filename,
                        offset
                    )
                }
            }
        }
        let frame = SymbolizedFrame {
            symbol_name: lookup_result.symbol.name,
            library_name: binary.filename.clone(),
            inlined,
            offset: offset as u64,
        };
        Ok(frame)
    }

    async fn load_binary(&mut self, filename: String, binary_addr: u64, first_segment_offset: u64) {
        let filepath = std::path::Path::new(&filename);
        log::info!("Processing {:?}", filepath);

        match self
            .symbol_manager
            .load_symbol_map_for_binary_at_path(filepath, None)
            .await
        {
            Ok(symbol_map) => {
                log::info!(
                    "Loaded symbol map for {:?}, start: 0x{:x}, first_segment: 0x{:x}, from: {:?}",
                    filepath,
                    binary_addr,
                    first_segment_offset,
                    symbol_map.symbol_file_origin()
                );
                self.symbol_maps.push(LoadedBinary {
                    filename: Arc::new(filename.clone()),
                    interval: Interval {
                        start: binary_addr,
                        size: 0,
                    },
                    first_segment_offset: first_segment_offset,
                    symbol_map: Some(symbol_map),
                });
            }
            Err(err) => {
                log::warn!(
                    "Failed to load symbol map for binary: {:?}, err: {}",
                    filepath,
                    err
                );
                self.symbol_maps.push(LoadedBinary {
                    filename: Arc::new(filename.clone()),
                    interval: Interval {
                        start: binary_addr,
                        size: 0,
                    },
                    first_segment_offset: first_segment_offset,
                    symbol_map: None,
                })
            }
        }
    }

    pub async fn update_symbols(&mut self, loaded_so: &[protos::ElfInfo]) {
        let orig_symbols_set =
            FxHashSet::from_iter(self.symbol_maps.iter().map(|x| x.filename.as_ref().clone()));
        let new_symbols_set = FxHashSet::from_iter(loaded_so.iter().map(|x| x.filename.clone()));
        let removed_symbols = orig_symbols_set
            .difference(&new_symbols_set)
            .collect::<FxHashSet<_>>();
        self.symbol_maps = self
            .symbol_maps
            .extract_if(.., |x| !removed_symbols.contains(x.filename.as_ref()))
            .collect::<Vec<_>>();
        let added_symbols = new_symbols_set
            .difference(&orig_symbols_set)
            .collect::<FxHashSet<_>>();

        for file in loaded_so
            .iter()
            .filter(|x| added_symbols.contains(&x.filename))
        {
            let first_segment_offset = match file.segments.first() {
                Some(segment) => segment.addr,
                None => {
                    log::error!(
                        "Failed to find first segment for {}, continue",
                        &file.filename
                    );
                    continue;
                }
            };
            self.load_binary(file.filename.clone(), file.addr, first_segment_offset)
                .await;
        }
        self.symbol_maps
            .sort_by(|lhs, rhs| lhs.interval.cmp(&rhs.interval));
    }
}

impl Default for Symbolizer {
    fn default() -> Self {
        Self::new()
    }
}
