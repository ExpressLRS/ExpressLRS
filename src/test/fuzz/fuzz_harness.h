#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

extern bool fuzzTrace;
// Prints to the per-packet trace, if FUZZ_TRACE is set
void trace(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

constexpr size_t FUZZ_HEADER_BYTES = 3; // test case setup, before the opcodes

// Once per process, before any fork
void fuzzInit();
// Boots the RX and plays one test case against it. The firmware's state cannot be reset, so this
// must only be called once per process.
void fuzzRunInput(const uint8_t *data, size_t size);

// Provided by the runner
[[noreturn]] void fuzzViolation(const std::string &kind, const std::string &detail);
