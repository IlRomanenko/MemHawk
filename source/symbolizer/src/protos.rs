#![allow(clippy::all)]
#![allow(warnings)]

use std::ops::{Add, AddAssign, Sub, SubAssign};

use serde::{Deserialize, Serialize, ser::SerializeStruct};
// Include the generated files from OUT_DIR
include!(concat!("../protos/memhawk.protos.rs"));

impl Sub for AllocSummary {
    type Output = AllocSummary;

    fn sub(self, rhs: Self) -> Self::Output {
        Self {
            size: self.size.wrapping_sub(rhs.size),
            active: self.active.wrapping_sub(rhs.active),
            overhead: self.overhead.wrapping_sub(rhs.overhead),
            total_count: self.total_count.wrapping_sub(rhs.total_count),
            total_bytes: self.total_bytes.wrapping_sub(rhs.total_bytes),
        }
    }
}

impl Add for AllocSummary {
    type Output = AllocSummary;

    fn add(self, rhs: Self) -> Self::Output {
        Self {
            size: self.size.wrapping_add(rhs.size),
            active: self.active.wrapping_add(rhs.active),
            overhead: self.overhead.wrapping_add(rhs.overhead),
            total_count: self.total_count.wrapping_add(rhs.total_count),
            total_bytes: self.total_bytes.wrapping_add(rhs.total_bytes),
        }
    }
}

impl AddAssign for AllocSummary {
    fn add_assign(&mut self, rhs: Self) {
        *self = *self + rhs;
    }
}

impl SubAssign for AllocSummary {
    fn sub_assign(&mut self, rhs: Self) {
        *self = *self - rhs;
    }
}
