#pragma once


#include <absl/types/span.h>

#include <cstdint>


// Simple bit-packing algorithm
void Compress(absl::Span<uint64_t> in, std::vector<uint32_t>& out);
void Decompress(absl::Span<uint32_t> in, std::vector<uint64_t>& out);
