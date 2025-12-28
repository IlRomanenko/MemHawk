use std::{cell::RefCell, rc::Rc, sync::Arc};

use crate::symbolizer::SymbolizedFrame;
use bitcode::{Decode, Encode};
use derive_more::{Add, AddAssign, From, Into};
use rustc_hash::FxHashMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq, From, Into, Hash, Add, AddAssign, Encode, Decode)]
pub struct LocalizedFrameId(u32);

pub const ROOT_LOCALIZED_ID: LocalizedFrameId = LocalizedFrameId(0);
pub const MEMHAWK_ROOT_LOCALIZED_ID: LocalizedFrameId = LocalizedFrameId(1);
pub const UNKNOWN_LOCALIZED_ID: LocalizedFrameId = LocalizedFrameId(2);

#[derive(Debug, Clone, Hash, PartialEq, Eq, Encode, Decode)]
pub struct LocalizedName {
    pub name: String,
    pub location: String,
    pub library: Arc<String>,
}

pub trait OnFrameLocalized {
    fn on_new_name(&mut self, name: &LocalizedName, name_id: LocalizedFrameId);
}

#[derive(Debug, Clone, Copy, Encode, Decode)]
pub struct VecRange {
    start: u32,
    end: u32,
}

impl VecRange {
    pub fn new(origin: &Vec<LocalizedFrameId>, size: usize) -> Self {
        VecRange {
            start: origin.len() as u32,
            end: (origin.len() + size) as u32,
        }
    }

    pub fn slice<'a>(&self, origin: &'a [LocalizedFrameId]) -> &'a [LocalizedFrameId] {
        &origin[self.start as usize..self.end as usize]
    }
}

#[derive(Encode, Decode)]
pub struct RestorableState {
    addr_to_localized_range: FxHashMap<u64, VecRange>,
    localized_frames: Vec<LocalizedFrameId>,
    localized_names: FxHashMap<LocalizedName, LocalizedFrameId>,
    next_frame_id: LocalizedFrameId,
}

pub struct FrameLocalizer {
    addr_to_localized_range: FxHashMap<u64, VecRange>,
    // addr -> [<name_id>, ... <name_id>]
    localized_frames: Vec<LocalizedFrameId>,

    // name -> name_id
    localized_names: FxHashMap<LocalizedName, LocalizedFrameId>,

    subscription: Rc<RefCell<dyn OnFrameLocalized>>,

    next_frame_id: LocalizedFrameId,
}

impl FrameLocalizer {
    pub fn new(subscription: Rc<RefCell<dyn OnFrameLocalized>>) -> Self {
        let mut localizer = Self {
            addr_to_localized_range: FxHashMap::default(),
            localized_frames: Vec::new(),
            localized_names: FxHashMap::default(),
            subscription,
            next_frame_id: UNKNOWN_LOCALIZED_ID + LocalizedFrameId::from(1),
        };

        localizer.add_predefined_node("root".to_owned(), ROOT_LOCALIZED_ID);
        localizer.add_predefined_node("memhawk".to_owned(), MEMHAWK_ROOT_LOCALIZED_ID);
        localizer.add_predefined_node("unknown".to_owned(), UNKNOWN_LOCALIZED_ID);
        localizer
    }

    pub fn stats(&self) {
        log::info!(
            "FrameLocalizer::addr_to_localized_range.len(): {}",
            self.addr_to_localized_range.len()
        );
        log::info!(
            "FrameLocalizer::localized_frames.len(): {}",
            self.localized_frames.len()
        );
        log::info!(
            "FrameLocalizer::localized_names.len(): {}",
            self.localized_names.len()
        );
    }

    fn add_predefined_node(&mut self, node_name: String, frame_id: LocalizedFrameId) {
        let localized_name = LocalizedName {
            name: node_name,
            location: "".to_owned(),
            library: Arc::default(),
        };
        self.subscription
            .borrow_mut()
            .on_new_name(&localized_name, frame_id);
        self.localized_names.insert(localized_name, frame_id);
    }

    pub fn save(&self) -> RestorableState {
        RestorableState {
            addr_to_localized_range: self.addr_to_localized_range.clone(),
            localized_frames: self.localized_frames.clone(),
            localized_names: self.localized_names.clone(),
            next_frame_id: self.next_frame_id,
        }
    }

    pub fn restore(
        state: RestorableState,
        subscription: Rc<RefCell<dyn OnFrameLocalized>>,
    ) -> Self {
        Self {
            addr_to_localized_range: state.addr_to_localized_range,
            localized_frames: state.localized_frames,
            localized_names: state.localized_names,
            subscription,
            next_frame_id: state.next_frame_id,
        }
    }

    pub fn get_frame_range(&self, addr: u64) -> Option<&VecRange> {
        self.addr_to_localized_range.get(&addr)
    }

    pub fn append_localized_range(&self, range: &VecRange, result: &mut Vec<LocalizedFrameId>) {
        assert!(
            self.localized_frames.len() >= range.end as usize,
            "Incorrect VecRange"
        );
        result.extend_from_slice(range.slice(&self.localized_frames))
    }

    pub fn process_symbolized_frame(&mut self, addr: u64, frame: SymbolizedFrame) -> VecRange {
        if frame.inlined.is_empty() {
            let range = VecRange::new(&self.localized_frames, 1);
            // use symbol itself
            let name = LocalizedName {
                name: frame.symbol_name,
                location: "?".to_owned(),
                library: frame.library_name.clone(),
            };
            self.add_new_localized_name(name);
            self.add_new_addr(addr, range);
            return range;
        }

        // process all inlined frames
        let range = VecRange::new(&self.localized_frames, frame.inlined.len());
        for inlined in frame.inlined {
            let name = LocalizedName {
                name: inlined.name,
                location: inlined.location,
                library: frame.library_name.clone(),
            };
            self.add_new_localized_name(name);
        }
        self.add_new_addr(addr, range);
        range
    }

    fn add_new_localized_name(&mut self, name: LocalizedName) {
        let new_name_id = self.next_frame_id;
        let name_id = *self
            .localized_names
            .entry(name.clone())
            .or_insert(new_name_id);
        // Have inserted?
        if name_id == new_name_id {
            self.next_frame_id += LocalizedFrameId::from(1);
            self.subscription.borrow_mut().on_new_name(&name, name_id);
        }
        self.localized_frames.push(name_id);
    }

    fn add_new_addr(&mut self, vaddr: u64, range: VecRange) {
        self.addr_to_localized_range
            .entry(vaddr)
            .insert_entry(range);
    }
}
