# ADR-0002: Expected errors use `tl::expected`; exceptions only in main

> 日本語版: [`docs/ja/adr/0002-error-handling.md`](../ja/adr/0002-error-handling.md)

- Status: Accepted
- Date: 2026-08-28

## Context

Static site generation runs into expected failures all the time — one file with broken YAML, for instance. Throwing an exception to escape makes it hard to collect which files were bad. Unexpected failures such as running out of memory, on the other hand, are naturally left as exceptions.

## Decision

- Expected errors return `tl::expected<T, kappan::Error>` (aliased as `kappan::Result<T>`). Library code never throws.
- `Error` is `{ code, message, where, line }`. The message states which file, what is wrong, and why, in Japanese.
- One failing file does not stop the whole build. The pipeline collects into a `std::vector<Error>` and reports everything at the end. If there is at least one, the exit code is non-zero.
- Only the top level of `src/main.cpp` may catch exceptions.

Phase 0 does not yet add `tl-expected` as a dependency. The type lands in `include/kappan/error.hpp` in Phase 1.

## Consequences

- The CLI can enumerate multiple errors.
- Tests can assert success and failure of a `Result` directly.
- Unexpected failures such as OOM are caught in main and exit non-zero.
