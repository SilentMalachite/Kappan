# ADR-0011: Distribute through GitHub Releases only, and verify self-containment with ctest

> 日本語版: [`docs/ja/adr/0011-release-distribution.md`](../ja/adr/0011-release-distribution.md)

- Status: Accepted
- Date: 2026-08-30
- Related: [ADR-0001](0001-cpp20-no-modules.md)

## Context

Of the goals in AGENTS.md §1, only "a single executable, distributable with zero dependencies, for macOS / Windows / Linux" was still unmet. Phases 0 through 7 delivered the features, but there was no way for a user to run kappan without setting up vcpkg, CMake, and a C++20 compiler.

The premise turned out to be broken too. The last five CI runs had all failed: Linux on an include that was only visible through libc++ and on `-Wmissing-field-initializers`, and Windows because the runner's MinGW was being picked up instead of MSVC. **`ctest` had never once run on Linux or Windows.** Since a release pipeline cannot be stacked on red CI, the three OSes were brought back to green first (8a). That work uncovered golden files corrupted by CRLF, the inability to delete an open file on Windows, and cmark-gfm exporting different target names per triplet.

Three things had to be decided: where to distribute, which environments to target, and how to guarantee self-containment.

## Decision

### 1. Distribute through GitHub Releases only

Pushing a `v*` tag runs `.github/workflows/release.yml`, which publishes four archives and a `SHA256SUMS`. No Homebrew tap, no Scoop bucket, no `curl | sh` install script, and no apt / dnf / winget registration.

### 2. Four targets, with floors fixed by measurement

| target | runner | Floor |
|---|---|---|
| `macos-arm64` | `macos-15` + Xcode 26.3 | macOS 15 |
| `macos-x86_64` | `macos-15-intel` + Xcode 26.3 | macOS 15 |
| `linux-x86_64` | `ubuntu-22.04` + g++-13 | glibc 2.35 |
| `windows-x86_64` | `windows-latest` + MSVC | The API sets present since Windows 8 |

arm64 Linux and arm64 Windows are not targeted.

The runners' default toolchains did not support these floors.

- The GCC on `ubuntu-22.04` tops out at 12.3, which has no `<format>` (that arrived in GCC 13). Moving to `ubuntu-24.04` would compile, but it would raise the glibc floor to 2.39, so the runner stays at 22.04 and g++-13 comes from the `ubuntu-toolchain-r/test` PPA.
- The libc++ of the default Xcode 16.4 on the `macos-15` images has no `std::jthread` or `std::stop_token`. Moving to the `macos-26` runners would compile, but it would raise the macOS floor to 26, so the runner stays at 15 and Xcode 26.3 is selected.

### 3. Self-containment is verified as a ctest test

`cmake/dist_selfcontained.cmake` runs under `cmake -P` and checks the output of `ldd` / `otool -L` / `dumpbin /dependents` against an allow list. It is registered only when `KAPPAN_DIST=ON`, and never lives in a CI shell step.

`cmake/dist_smoke.cmake` makes the executable itself generate `examples/` and compares the result byte for byte against `tests/golden/*/expected`. It is registered regardless of `KAPPAN_DIST`, so it also runs under `ctest --preset dev`.

### 4. Link settings are collected under `KAPPAN_DIST`

| Platform | Setting |
|---|---|
| MSVC | The `x64-windows-static` vcpkg triplet, with the CRT as `MultiThreaded` (`/MT`) |
| Linux | `-static-libstdc++ -static-libgcc`; glibc stays dynamic |
| macOS | Nothing extra |

### 5. macOS binaries are neither signed nor notarised

## Rationale

- **Package managers** would mean creating separate repositories and automating manifest updates. That is not a maintenance burden to take on before the distribution path is known to work.
- **arm64 Linux and Windows** cannot be verified here. We do not ship what we cannot verify.
- **Linking glibc statically** produces warnings around `getaddrinfo` and would force extra verification of `serve`. A glibc 2.35 floor is enough in practice, and linking only libstdc++ and libgcc statically means the binary does not demand a newer GCC than the build environment had.
- **Signing** requires the annual Apple Developer Program fee and key management. The difference for the person receiving the binary is one `xattr -d com.apple.quarantine` command, which does not justify the cost.
- **Putting verification in a CI shell** cannot be reproduced locally and doubles into a bash version and a pwsh version. In ctest the same verification runs in both places, and it stays as a test asset.
- **The version gate** exists because a missed update to `project(kappan VERSION ...)` or `vcpkg.json` would only surface at release time. The value derived from the tag is compared against `kappan --version`, and a mismatch stops the job.

## Rejected alternatives

**Write the verification directly as shell steps in `release.yml`.** Not reproducible locally, and it doubles into bash and pwsh versions. The verification of the artifacts would not survive as a test asset.

**An external release tool such as goreleaser.** Go-centric and a poor fit for a vcpkg C++ build. It would also count as adding a dependency under AGENTS.md §11.

**Fully static with musl.** The best portability available, but it requires a new vcpkg triplet and build environment — too heavy for the portability gained.

**Move to `ubuntu-24.04` and accept a glibc 2.39 floor.** The workflow gets simpler, but Ubuntu 22.04 and Debian 12 users fall off. Adding one PPA costs less.

**Use the `macos-26` runners.** Equally simple, but the macOS floor becomes 26. The runner's OS is a build environment, not a distribution target, so re-selecting Xcode is the sounder move.

**Tag `v1.0.0` from the start.** That would claim v1 while the distribution path was still unverified. We push `v0.1.0` through it once first.

**Ship a macOS universal binary.** That adds a lipo step in CI. Shipping arm64 and x86_64 separately meets the requirement.

## Consequences

- Pushing a `v0.1.0` tag is enough to put four archives and a `SHA256SUMS` on Releases.
- The measured dependencies are below, and all of them ship with the target environment.

  | Environment | Dependencies |
  |---|---|
  | macOS | `CFNetwork`, `CoreFoundation`, `/usr/lib/libc++.1.dylib`, `/usr/lib/libSystem.B.dylib` |
  | Linux | `libm.so.6`, `libc.so.6`, `ld-linux-x86-64.so.2` (the highest glibc symbol required is `GLIBC_2.35`) |
  | Windows | `KERNEL32.dll`, `WS2_32.dll`, `api-ms-win-core-synch-l1-2-0.dll` |

- macOS users need `xattr -d com.apple.quarantine ./kappan` once. The README documents it.
- Environments below glibc 2.35 (RHEL 9 is at 2.34) have to build from source.
- We depend on the `ubuntu-toolchain-r/test` PPA and on `ilammy/msvc-dev-cmd`. The latter is pinned by commit SHA.
- 8a added `.gitattributes` to pin the working tree to LF, because the CRLF conversion performed by checkout on Windows was corrupting both the byte-compared golden files and the theme that gets embedded into the binary.
