#![allow(clippy::all)]
#![allow(warnings)]

use std::ops::{Add, AddAssign, Sub, SubAssign};

use bitcode::{Decode, Encode};
use serde::{Deserialize, Serialize, ser::SerializeStruct};
// Include the generated files from OUT_DIR
include!(concat!("gen/memhawk.proto.rs"));

#[derive(Debug, Clone, Copy, Encode, Decode)]
pub struct AllocDiff {
    pub size: i64,
    pub active: i64,
    pub overhead: i64,
    pub total_count: u64,
    pub total_bytes: u64,
}

impl Add for AllocDiff {
    type Output = AllocDiff;

    fn add(self, rhs: Self) -> Self::Output {
        Self::Output {
            size: self.size.wrapping_add(rhs.size),
            active: self.active.wrapping_add(rhs.active),
            overhead: self.overhead.wrapping_add(rhs.overhead),
            total_count: self.total_count.wrapping_add(rhs.total_count),
            total_bytes: self.total_bytes.wrapping_add(rhs.total_bytes),
        }
    }
}

impl Sub for AllocDiff {
    type Output = AllocDiff;

    fn sub(self, rhs: Self) -> Self::Output {
        Self::Output {
            size: self.size.wrapping_sub(rhs.size),
            active: self.active.wrapping_sub(rhs.active),
            overhead: self.overhead.wrapping_sub(rhs.overhead),
            total_count: self.total_count.wrapping_sub(rhs.total_count),
            total_bytes: self.total_bytes.wrapping_sub(rhs.total_bytes),
        }
    }
}

impl AddAssign for AllocDiff {
    fn add_assign(&mut self, rhs: Self) {
        *self = *self + rhs;
    }
}

impl SubAssign for AllocDiff {
    fn sub_assign(&mut self, rhs: Self) {
        *self = *self - rhs;
    }
}

impl From<AllocDiff> for AllocSummary {
    fn from(value: AllocDiff) -> Self {
        Self {
            size: value.size,
            active: value.active,
            overhead: value.overhead,
            total_count: value.total_count,
            total_bytes: value.total_bytes,
        }
    }
}

impl Add<AllocDiff> for AllocSummary {
    type Output = AllocSummary;

    fn add(self, rhs: AllocDiff) -> Self::Output {
        Self::Output {
            size: self.size.wrapping_add(rhs.size),
            active: self.active.wrapping_add(rhs.active),
            overhead: self.overhead.wrapping_add(rhs.overhead),
            total_count: self.total_count.wrapping_add(rhs.total_count),
            total_bytes: self.total_bytes.wrapping_add(rhs.total_bytes),
        }
    }
}

impl Sub<AllocDiff> for AllocSummary {
    type Output = AllocSummary;

    fn sub(self, rhs: AllocDiff) -> Self::Output {
        Self::Output {
            size: self.size.wrapping_sub(rhs.size),
            active: self.active.wrapping_sub(rhs.active),
            overhead: self.overhead.wrapping_sub(rhs.overhead),
            total_count: self.total_count.wrapping_sub(rhs.total_count),
            total_bytes: self.total_bytes.wrapping_sub(rhs.total_bytes),
        }
    }
}

impl Sub for AllocSummary {
    type Output = AllocDiff;

    fn sub(self, rhs: Self) -> Self::Output {
        Self::Output {
            size: self.size.wrapping_sub(rhs.size),
            active: self.active.wrapping_sub(rhs.active),
            overhead: self.overhead.wrapping_sub(rhs.overhead),
            total_count: self.total_count.wrapping_sub(rhs.total_count),
            total_bytes: self.total_bytes.wrapping_sub(rhs.total_bytes),
        }
    }
}

impl AddAssign<AllocDiff> for AllocSummary {
    fn add_assign(&mut self, rhs: AllocDiff) {
        *self = *self + rhs;
    }
}

impl SubAssign<AllocDiff> for AllocSummary {
    fn sub_assign(&mut self, rhs: AllocDiff) {
        *self = *self - rhs;
    }
}

#[derive(Clone, Copy, PartialEq, Eq, strum::EnumIter)]
pub enum ValueType {
    ActiveSize = 1,
    ActiveCount = 2,
    TotalSize = 3,
    TotalCount = 4,
    Overhead = 5,
}

pub fn value_selector(summary: &AllocSummary, value_type: ValueType) -> i64 {
    match value_type {
        ValueType::ActiveSize => summary.size,
        ValueType::ActiveCount => summary.active,
        ValueType::TotalCount => summary.total_count as i64,
        ValueType::TotalSize => summary.total_bytes as i64,
        ValueType::Overhead => summary.overhead as i64,
    }
}
