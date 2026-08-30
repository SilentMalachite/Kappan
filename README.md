# Kappan (活版)

[![CI](https://github.com/SilentMalachite/Kappan/actions/workflows/ci.yml/badge.svg)](https://github.com/SilentMalachite/Kappan/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/SilentMalachite/Kappan?sort=semver)](https://github.com/SilentMalachite/Kappan/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](docs/adr/0001-cpp20-no-modules.md)

A single-binary static site generator in C++20 that turns Markdown into blog posts and landing pages, built for Japanese content.

*日本語版: [README.ja.md](README.ja.md)*

## Why Kappan

- **One executable, no runtime.** No Node, no Python, no Ruby. The default theme is embedded in the binary, so there are no data files to install alongside it.
- **Japanese first.** Kana, kanji, emoji, and mixed halfwidth/fullwidth text survive from filenames through slugs to URLs. Nothing is romanised and Japanese URLs are not percent-encoded.
- **Errors that name the file.** Expected failures say which file, what is wrong, and why, with a line number. One broken file does not abort the build; every error is collected and reported at the end.
- **Live preview that does not lie.** `serve --watch` rebuilds on save, and when a rebuild fails it keeps serving the last known-good generation instead of a broken page.
- **Byte-exact output.** Golden tests compare the generated site byte for byte, including the CLI path taken by the shipped binary.

## Install

Download the archive for your platform from [Releases](https://github.com/SilentMalachite/Kappan/releases).

| Platform | Archive |
|---|---|
| macOS 15+ (Apple Silicon) | `kappan-<version>-macos-arm64.tar.gz` |
| macOS 15+ (Intel) | `kappan-<version>-macos-x86_64.tar.gz` |
| Linux x86_64 (glibc 2.35+) | `kappan-<version>-linux-x86_64.tar.gz` |
| Windows x64 | `kappan-<version>-windows-x86_64.zip` |

Put `SHA256SUMS` from the same page next to the archive and verify it.

```bash
sha256sum -c SHA256SUMS --ignore-missing   # macOS: shasum -a 256 -c
```

Extracting gives you `kappan-<version>-<target>/` containing the executable, `README.md`, and `LICENSE`. Move the executable somewhere on your `PATH`.

```bash
tar -xzf kappan-0.1.0-macos-arm64.tar.gz
sudo mv kappan-0.1.0-macos-arm64/kappan /usr/local/bin/
```

On macOS the distributed binaries are not signed (see [ADR-0011](docs/adr/0011-release-distribution.md)), so Gatekeeper blocks them based on the quarantine attribute. Clear it once.

```bash
xattr -d com.apple.quarantine /usr/local/bin/kappan
```

Check the install:

```bash
kappan --version   # 0.1.0
```

The theme is embedded in the binary, so there are no extra data files to place.

## Quick start

```bash
kappan new my-site
kappan serve --source my-site --watch     # http://127.0.0.1:8080
kappan build --source my-site --out _site
```

A site root is a directory holding `site.yaml`:

```
my-site/
├── site.yaml          # title, url, language, pagination
├── content/
│   ├── index.md       # home page (layout: index)
│   ├── about.md       # → /about/
│   └── posts/
│       └── 2026-01-01-hello.md   # → /posts/hello/
├── static/            # copied verbatim to the output root
└── templates/         # optional; overrides the bundled theme
```

## Commands

| Command | What it does |
|---|---|
| `kappan build --source <dir> --out <dir>` | Site root → HTML, plus `static/`, `sitemap.xml`, and `feed.xml` |
| `kappan serve --source <dir> [--watch]` | Serve over loopback; `--watch` rebuilds on save and reloads the browser |
| `kappan new <dir>` | Write a site skeleton into an empty directory |

`--drafts` includes `draft: true` documents. `--force` lets `build` empty an output directory that kappan did not create. Full details are in [docs/spec/cli.md](docs/spec/cli.md).

```bash
kappan build --source examples/blog --out _site
kappan build --source examples/landing --out _site
```

## Documentation

English is canonical; each page links to its Japanese counterpart.

| Topic | Page |
|---|---|
| CLI reference | [docs/spec/cli.md](docs/spec/cli.md) |
| `site.yaml` | [docs/spec/site-yaml.md](docs/spec/site-yaml.md) |
| Front matter | [docs/spec/front-matter.md](docs/spec/front-matter.md) |
| Collections, tags, pagination | [docs/spec/collections.md](docs/spec/collections.md) |
| Templates and variables | [docs/spec/templates.md](docs/spec/templates.md) |
| Landing pages and OGP | [docs/spec/landing.md](docs/spec/landing.md) |
| Output, sitemap, RSS | [docs/spec/output.md](docs/spec/output.md) |
| Architecture decisions | [docs/adr/](docs/adr/) |
| Changes | [CHANGELOG.md](CHANGELOG.md) |
| Contributing | [CONTRIBUTING.md](CONTRIBUTING.md) |

## Build from source

Requirements:

- CMake 3.28+
- Ninja
- A C++20 compiler. Kappan uses `std::format` and `std::jthread`, so anything older than these will not compile:
  - macOS: Xcode 26+ (the libc++ in Xcode 16.4 has no `std::jthread`)
  - Linux: GCC 13+ (GCC 12 has no `<format>`)
  - Windows: MSVC (MinGW and Clang do not match the ABI of the vcpkg `x64-windows` triplet)
- [vcpkg](https://github.com/microsoft/vcpkg), as a sibling directory `../vcpkg`

```bash
# first time only
git clone https://github.com/microsoft/vcpkg.git ../vcpkg
../vcpkg/bootstrap-vcpkg.sh          # Windows: bootstrap-vcpkg.bat

cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Release build:

```bash
cmake --preset release
cmake --build --preset release
```

To build the self-contained binary the release archives use, see [CONTRIBUTING.md](CONTRIBUTING.md#distribution-builds).

To point at a vcpkg in a different location, set `VCPKG_ROOT` in `CMakeUserPresets.json`, which is gitignored.

## Security

Kappan enables cmark-gfm's `tagfilter` extension, which neutralises dangerous HTML tags, but raw HTML in Markdown is otherwise passed through. Treat the contents of `content/` as trusted input. See [SECURITY.md](SECURITY.md) to report a vulnerability.

## License

[MIT](LICENSE)
