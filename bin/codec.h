#pragma once

#include <cstdint>
#include <ostream>

void HammingEncodeStream(std::istream& in, std::ostream& out, uint64_t originalSize);

bool HammingDecodeStream(std::istream& in, std::ostream& out, uint64_t originalSize);