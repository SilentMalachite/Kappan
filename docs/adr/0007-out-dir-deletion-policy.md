# ADR-0007: When `--out` may be deleted

> 日本語版: [`docs/ja/adr/0007-out-dir-deletion-policy.md`](../ja/adr/0007-out-dir-deletion-policy.md)

- Status: Accepted
- Date: 2026-08-29
- Related: [ADR-0006](0006-output-assets-feeds.md) (this ADR supersedes its handling of `--out`)

## Context

The Phase 5 review measured that `output::prepare_out_dir` calls `remove_all` on `--out` unconditionally.

ADR-0006 only decided that "`--out` is emptied before writing; if `--out` equals the source root, or the source lives inside `--out`, fail with `ErrorCode::Cli`", and `docs/spec/output.md` describes the purpose of that guard as "so that the source is never deleted". In other words the only thing protected is `--source`, and **an `--out` unrelated to `--source` is not protected at all**.

Measured with `build/dev/kappan`:

```
Against a directory holding precious/photos/wedding.jpg and precious/docs/2025.pdf:
kappan build --source s --out precious
→ exit=0, no confirmation, no warning, both deleted recursively
```

`--out` is a required option with no default (`src/main.cpp:29`), so a typo or an undefined shell variable expands straight into an unrecoverable deletion. All three checks in `prepare_out_dir` look only at the relationship between `src` and `out`; when `src` is outside `out`, `rel` contains `..` and everything passes.

A simple "refuse when non-empty" does not work: `--out` is always non-empty from the previous build's output, so `--force` would be needed every time.

## Decision

**Write a marker file at the output root, and never delete a non-empty directory that lacks a valid marker.**

- The marker is `<out>/.kappan-out`. Its content is the fixed single line `kappan output directory` plus a newline. No timestamp and no version, so it produces no diff in the golden files.
- A marker is valid only when, in this order, the directory entry is not a symbolic link, it is a regular file, and its raw bytes match exactly `kappan output directory\n` (24 bytes). No BOM removal or newline normalisation is applied. If its status or content cannot be inspected, fail with `ErrorCode::Io` and delete nothing; a confirmed invalid marker fails with `ErrorCode::Cli` and deletes nothing.
- The order of decisions in `prepare_out_dir`:
  1. The three existing checks (`--out` == source, source inside `--out`, `--out` is an existing file). A refusal here **deletes nothing**, as before.
  2. `--out` does not exist → create it.
  3. `--out` is empty → use it as-is.
  4. `--out` is non-empty and has a valid `.kappan-out` → `remove_all` + `create_directories`, as before.
  5. `--out` is non-empty and `.kappan-out` is absent or confirmed invalid → refuse with `ErrorCode::Cli` and **delete nothing**.
  6. The output-directory emptiness, marker status, or marker content cannot be inspected → fail with `ErrorCode::Io` and **delete nothing**.
- The marker is written immediately after `create_directories`, so that a build failing part-way through still lands in case 4 on the next run.
- `build` gains a `--force` flag, which bypasses marker validation and deletes even in case 5. `--force` does not affect the three checks in case 1 — the source is always protected.
- The message for case 5 says what to do next:

  ```
  {out}: kappan の出力先ではないディレクトリが空ではありません。消してよければ --force を付けてください
  ```

## Rejected alternatives

**Keep a manifest of the previous output and delete only our own files.** This would remove the need for `remove_all` and keep the marker out of the output, but it runs into the same problem for where the manifest itself lives, and reconciling it after a manual deletion or a partial failure is hard. Worth revisiting if the Phase 6 incremental build needs a manifest anyway.

## Consequences

- `kappan build --out ~` is refused on the first run. That is the point of this ADR.
- Workflows that write to a fresh `--out` every time, such as CI, are unaffected because the directory is empty.
- One extra file, `.kappan-out`, appears in the output, including the golden files in `tests/golden/blog-ja/expected/`. It starts with a dot, so ordinary web servers do not serve it.
- A user placing their own `static/.kappan-out` does not collide: `copy_static` excludes dot-prefixed entries from traversal (see the static assets section of `docs/spec/output.md`), so it never reaches the output.
- `docs/spec/output.md` (preparing the output directory) and `docs/spec/cli.md` (`--force`) are updated.
