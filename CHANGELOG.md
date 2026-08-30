# Changelog

The format of this file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versioning follows [Semantic Versioning](https://semver.org/).

*日本語版: [CHANGELOG.ja.md](CHANGELOG.ja.md)*

## [0.1.1] - 2026-08-30

Documentation only. The binary behaves exactly as 0.1.0 does.

### Changed

- The `README.md` bundled in the release archives is now the English one, which is
  canonical from this release on. Japanese lives alongside it as `README.ja.md`
- `docs/spec/` and `docs/adr/` are English; `docs/ja/` mirrors them in Japanese

### Added

- `CONTRIBUTING.md`, `SECURITY.md`, and `CODE_OF_CONDUCT.md`, each with a Japanese counterpart
- Issue forms and a pull request template

### Fixed

- The compiler requirements stated in the README and `AGENTS.md` were wrong. Phase 8
  measured the real floors: Xcode 26+ on macOS, GCC 13+ on Linux, and MSVC on Windows.
  Xcode 16.4 has no `std::jthread` and GCC 12 has no `<format>`

## [0.1.0] - 2026-08-30

First release.

> **The documentation bundled in the 0.1.0 archives is superseded by 0.1.1.**
> The `README.md` inside them predates English becoming canonical, and it states compiler
> requirements that later turned out to be wrong. The executables are unaffected: 0.1.1
> changes documentation only and behaves identically. Prefer
> [v0.1.1](https://github.com/SilentMalachite/Kappan/releases/tag/v0.1.1).

### Added

- `kappan build` — generates a static site from a site root containing `site.yaml`.
  Reads Markdown (GFM) with YAML front matter and writes HTML at pretty URLs
- `kappan serve` — serves the generated site over loopback. `--watch` detects saves,
  rebuilds, and reloads the browser. When a rebuild fails, it keeps serving the last
  known-good generation
- `kappan new` — writes a site skeleton into an empty directory
- Collections, tag listings, and pagination
- Static asset copying, `sitemap.xml`, and `feed.xml`
- Landing pages via `layout: landing`, composed from the `sections:` front matter
- OGP meta output
- The bundled theme (`base` / `post` / `page` / `index` / `tag` / `landing`) embedded in the binary
- Self-contained binaries for macOS, Linux, and Windows, distributed through GitHub Releases

### Supported platforms

| Archive | Runs on |
|---|---|
| `kappan-0.1.0-macos-arm64.tar.gz` | macOS 15+ (Apple Silicon) |
| `kappan-0.1.0-macos-x86_64.tar.gz` | macOS 15+ (Intel) |
| `kappan-0.1.0-linux-x86_64.tar.gz` | x86_64 Linux with glibc 2.35+ |
| `kappan-0.1.0-windows-x86_64.zip` | Windows x64 |

### Notes

- Kappan is verified against input containing Japanese (kana and kanji), emoji, and mixed
  halfwidth/fullwidth text
- macOS binaries are not signed. The quarantine attribute has to be cleared before the first
  run; see the Install section of the README
- The distribution decisions are recorded in [ADR-0011](docs/adr/0011-release-distribution.md)

[0.1.1]: https://github.com/SilentMalachite/Kappan/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/SilentMalachite/Kappan/releases/tag/v0.1.0
