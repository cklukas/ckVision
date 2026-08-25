// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "todo_codec.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);
    const ckv::todo::TodoDecodeResult decoded = ckv::todo::decode_workspace(input);
    if (!decoded) return 0;

    const ckv::todo::TodoEncodeResult canonical = ckv::todo::encode_workspace(*decoded.value);
    ckv::fuzz::require(static_cast<bool>(canonical));
    const ckv::todo::TodoDecodeResult decoded_canonical = ckv::todo::decode_workspace(*canonical.value);
    ckv::fuzz::require(static_cast<bool>(decoded_canonical));
    ckv::fuzz::require(decoded_canonical.value->snapshot() == decoded.value->snapshot());
    const ckv::todo::TodoEncodeResult second = ckv::todo::encode_workspace(*decoded_canonical.value);
    ckv::fuzz::require(static_cast<bool>(second));
    ckv::fuzz::require(*second.value == *canonical.value);
    return 0;
}
