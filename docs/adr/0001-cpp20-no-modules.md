# ADR-0001: Use C++20, but not modules

> 日本語版: [`docs/ja/adr/0001-cpp20-no-modules.md`](../ja/adr/0001-cpp20-no-modules.md)

- Status: Accepted
- Date: 2026-08-28

## Context

Kappan produces a single binary from the same source on macOS, Windows, and Linux. C++20 gives us `std::format`, `std::filesystem`, concepts, and `std::span` from the standard library. C++20 modules, on the other hand, are supported very unevenly across compilers, CMake, and vcpkg.

## Decision

The language is **C++20**, and **modules are not used**. We keep the conventional header plus translation unit layout. Coroutines are also out of scope for v1.

## Consequences

- We can assume the same code compiles on the toolchains listed in the README.
- The public API lives in `include/kappan/*.hpp` and the implementation in `src/`.
- Moving to modules would mean superseding this ADR.

> Note (2026-08-30): this ADR originally claimed AppleClang 15+ / GCC 13+ / MSVC 19.38+. Phase 8 measured the real floors, which are higher on macOS. See the README and [ADR-0011](0011-release-distribution.md).
