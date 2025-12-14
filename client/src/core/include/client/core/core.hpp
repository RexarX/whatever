#pragma once

#include <utility>

// Debug break functionality
#ifdef CLIENT_ENABLE_ASSERTS

// TODO: Replace with std::breakpoint() from <debugging> in C++26
// https://en.cppreference.com/w/cpp/utility/breakpoint.html

// MSVC: Use the intrinsic which maps to the appropriate inlined assembly
#if defined(_MSC_VER) && (_MSC_VER >= 1300)
#define CLIENT_DEBUG_BREAK() __debugbreak()

// ARM64 on Apple: Use signal-based break (better debugger integration)
#elif defined(__arm64__) && defined(__APPLE__)
#include <unistd.h>
#include <csignal>
#define CLIENT_DEBUG_BREAK() ::kill(::getpid(), SIGINT)

// ARM64 with GCC/Clang: Use brk instruction
#elif defined(__arm64__) && (defined(__GNUC__) || defined(__clang__))
#define CLIENT_DEBUG_BREAK() __asm__("brk 0")

// ARM (32-bit) on Apple: Use trap instruction
#elif defined(__arm__) && defined(__APPLE__)
#define CLIENT_DEBUG_BREAK() __asm__("trap")

// ARM (32-bit) with GCC/Clang: Use bkpt instruction
#elif defined(__arm__) && (defined(__GNUC__) || defined(__clang__))
#define CLIENT_DEBUG_BREAK() __asm__("bkpt 0")

// ARM with Arm Compiler: Use breakpoint intrinsic
#elif defined(__arm__) && defined(__ARMCC_VERSION)
#define CLIENT_DEBUG_BREAK() __breakpoint(0)

// x86/x86_64 with Clang/GCC: Use int3 instruction (AT&T syntax)
#elif (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#define CLIENT_DEBUG_BREAK() __asm__("int3")

// x86/x86_64 with MSVC: Use int3 instruction (Intel syntax)
#elif (defined(_M_X64) || defined(_M_IX86)) && defined(_MSC_VER)
#define CLIENT_DEBUG_BREAK() __asm int 3

// PowerPC: Trigger exception via opcode 0x00000000
#elif defined(__powerpc__) || defined(__ppc__) || defined(_ARCH_PPC)
#define CLIENT_DEBUG_BREAK() __asm__(".long 0")

// WebAssembly: No breakpoint support, use unreachable
#elif defined(__wasm__)
#define CLIENT_DEBUG_BREAK() __asm__("unreachable")

// Fallback: Use compiler builtin trap (works on most modern compilers)
#elif defined(__has_builtin) && __has_builtin(__builtin_trap)
#define CLIENT_DEBUG_BREAK() __builtin_trap()

// Last resort: Use signal-based approach
#else
#include <csignal>
#define CLIENT_DEBUG_BREAK() ::std::raise(SIGTRAP)

#endif

#else
// Debug break disabled in release builds
#define CLIENT_DEBUG_BREAK()
#endif

#define CLIENT_UNREACHABLE() std::unreachable()

// Release mode unreachable
#ifndef CLIENT_DEBUG_MODE
#define CLIENT_RELEASE_UNREACHABLE() CLIENT_UNREACHABLE()
#else
#define CLIENT_RELEASE_UNREACHABLE()
#endif

// Utility macros
#define CLIENT_BIT(x) (1 << (x))

// Stringify macros
#define CLIENT_STRINGIFY_IMPL(x) #x
#define CLIENT_STRINGIFY(x) CLIENT_STRINGIFY_IMPL(x)

// Concatenation macros
#define CLIENT_CONCAT_IMPL(a, b) a##b
#define CLIENT_CONCAT(a, b) CLIENT_CONCAT_IMPL(a, b)

// Anonymous variable generation
#define CLIENT_ANONYMOUS_VAR(prefix) CLIENT_CONCAT(prefix, __LINE__)

// Force inline macros
#ifdef _MSC_VER
#define CLIENT_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CLIENT_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define CLIENT_FORCE_INLINE inline
#endif

// No inline macros
#ifdef _MSC_VER
#define CLIENT_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CLIENT_NO_INLINE __attribute__((noinline))
#else
#define CLIENT_NO_INLINE
#endif

// Compiler-specific branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define CLIENT_EXPECT_TRUE(x) __builtin_expect(!!(x), 1)
#define CLIENT_EXPECT_FALSE(x) __builtin_expect(!!(x), 0)
#else
#define CLIENT_EXPECT_TRUE(x) (x)
#define CLIENT_EXPECT_FALSE(x) (x)
#endif
