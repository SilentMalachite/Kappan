# Contributing to Kappan

Thanks for taking an interest. This page covers everything you need to get a change merged.

*日本語版: [CONTRIBUTING.ja.md](CONTRIBUTING.ja.md)*

`AGENTS.md` is the authoritative engineering handbook for this repository. It is written in Japanese and goes deeper than this page; where the two disagree, `AGENTS.md` wins.

## Before you start

- For a bug, open an issue with a minimal reproduction: the input site, the command you ran, what you expected, and what happened.
- For a feature, open an issue first. Kappan deliberately keeps its scope small, and a design discussion before the code saves everyone time.
- For a change to the public headers under `include/kappan/`, record the decision in `docs/adr/` before writing the code.

## Development setup

```bash
git clone https://github.com/SilentMalachite/Kappan.git
cd Kappan

# vcpkg must be a sibling directory
git clone https://github.com/microsoft/vcpkg.git ../vcpkg
../vcpkg/bootstrap-vcpkg.sh          # Windows: bootstrap-vcpkg.bat

cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

To point at a vcpkg elsewhere, set `VCPKG_ROOT` in `CMakeUserPresets.json`, which is gitignored.

**Never introduce a build command outside the presets.** If you need one, add it to `CMakePresets.json`.

| Preset | Purpose |
|---|---|
| `dev` | Debug build. Warnings are not errors locally; CI turns them on |
| `release` | Optimised build, warnings as errors |
| `dist` / `dist-windows` | Self-contained build for distribution (see below) |

## Coding standards

- **C++20, no modules and no coroutines** ([ADR-0001](docs/adr/0001-cpp20-no-modules.md)).
- **Expected errors return `kappan::Result<T>`** (`tl::expected`) instead of escaping as
  exceptions. Catch exceptions only at the top level of `src/main.cpp`, at the nearest boundary
  around an exception-throwing external library, or at a worker-thread entry that converts failure
  and returns it after joining. Never swallow an exception and continue. Error messages state which
  file, what is wrong, and why, with a line number.
- **Ownership is values and `std::unique_ptr`.** No raw `new` / `delete`. Non-owning references are `const T&`, `std::string_view`, or `std::span`.
- **Paths are always `std::filesystem::path`.** Never build a path by concatenating strings.
- **Prefer `std::ranges` and views over hand-written loops**, `concepts` over SFINAE, and `std::format` over `printf` or iostream formatting.
- Types are `PascalCase`, functions and variables `snake_case`, members end with `_`, constants are `kPascalCase`. Headers use `#pragma once`. Aim for 50 lines per function and 400 lines per file.
- **Never silence a warning with `#pragma`**, and never work around one by disabling `-Werror` or `/WX`. If a warning genuinely cannot be removed, leave a comment explaining why and raise it in the issue or PR.
- Format with `cmake --build --preset dev --target format`, never by hand.
- Identifiers and filenames are English. Comments and documentation follow the language of the file you are editing.

## Tests

Tests are Catch2 v3, run through `ctest --preset dev`.

- **A feature without a test is not finished.** Every new or changed behaviour needs one.
- Golden tests live in `tests/golden/<case>/`, holding an input site and an `expected/` tree compared byte for byte.
- **Fixtures must include Japanese (kana and kanji), emoji, and mixed halfwidth/fullwidth text.** This is not optional; it is the core of what Kappan promises.
- Always cover: UTF-8 round-tripping (no BOM added, nothing corrupted), slugs derived from Japanese filenames and headings, CRLF input from Windows, and error reporting that does not throw when front matter is broken.

Two tests run outside the Catch2 binary:

| Test | What it checks |
|---|---|
| `dist_smoke` | Runs the built executable against `examples/` and compares the output to the golden files. Registered in every preset, so it covers the CLI path |
| `dist_selfcontained` | Checks the distributed binary's shared-library dependencies against an allow list. Registered only when `KAPPAN_DIST=ON` |

## Distribution builds

The self-contained binaries in the release archives are produced by the `dist` presets.

```bash
cmake --preset dist                 # Windows: --preset dist-windows
cmake --build --preset dist
ctest --preset dist --output-on-failure
cmake --install build/dist --prefix stage
```

This links libstdc++ and libgcc statically on Linux and uses the `x64-windows-static` vcpkg triplet with `/MT` on Windows. `ctest --preset dist` includes `dist_selfcontained`, which fails if the binary picks up a dependency outside the allow list. The reasoning is in [ADR-0011](docs/adr/0011-release-distribution.md).

## Dependencies

**Do not add a dependency without agreement.** Dependencies are managed through vcpkg manifest mode (`vcpkg.json`). If you believe one is needed, open an issue with the alternatives you considered, the effect on binary size, and the license.

## Commits and pull requests

One commit, one purpose. Do not mix unrelated formatting or renames into a change.

```
<type>: <summary, 50 characters or fewer>

<why this change is needed. What you did is visible in the diff; the reason is not>
```

`type` is one of `feat`, `fix`, `refactor`, `test`, `docs`, `build`, `chore`, `perf`, or `ci`.

Before opening a pull request, confirm all of the following:

- [ ] `cmake --build --preset dev` passes with zero warnings
- [ ] `ctest --preset dev` passes completely
- [ ] New or changed behaviour has a test
- [ ] You verified the change against input containing Japanese
- [ ] You can summarise the change in five lines or fewer

CI builds on macOS, Linux, and Windows with warnings as errors. All three have to be green.

## Documentation

English is canonical. `docs/spec/` and `docs/adr/` hold the English pages, and `docs/ja/` mirrors them in Japanese.

When you change behaviour, update the matching page under `docs/spec/`. When you make a design
decision, add an ADR under `docs/adr/`. Update the corresponding Japanese page under `docs/ja/` in
the same change; do not update only one language.

## Code of conduct

Participation is governed by the [Code of Conduct](CODE_OF_CONDUCT.md).
