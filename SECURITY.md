# Security Policy

*日本語版: [SECURITY.ja.md](SECURITY.ja.md)*

## Supported versions

Kappan is pre-1.0. Only the latest release receives fixes.

| Version | Supported |
|---|---|
| 0.1.x | ✅ |
| < 0.1 | ❌ |

## Reporting a vulnerability

**Please do not open a public issue for a security problem.**

Use GitHub's private vulnerability reporting:
[Report a vulnerability](https://github.com/SilentMalachite/Kappan/security/advisories/new).

Please include:

- The version (`kappan --version`) and platform
- A minimal reproduction: the input site, the command, and the observed behaviour
- What an attacker gains, as concretely as you can state it

You should get an initial response within a week. If the report is accepted, we will agree a disclosure timeline with you and credit you in the advisory unless you prefer otherwise.

## Threat model

Kappan is a command-line tool that reads local files and writes local files. Understanding what is and is not in scope will save you time.

### In scope

- Writing outside `--out` — path traversal through a permalink, slug, or `static/` entry
- Deleting anything outside `--out`; deleting the source root or one of its ancestors through
  `--out`, even with `--force`; or deleting a non-empty `--out` without a valid `.kappan-out`
  marker unless the user explicitly passed `--force`
  ([ADR-0007](docs/adr/0007-out-dir-deletion-policy.md))
- `kappan serve` serving a file outside the generated site, or binding beyond loopback without an explicit `--host`
- A crash, hang, or unbounded memory growth on malformed but plausible input
- Anything in a release archive that does not match what the tagged source builds

### Out of scope

- **Raw HTML and script in Markdown.** Kappan enables cmark-gfm's `tagfilter` extension, which neutralises dangerous tags, but raw HTML otherwise passes through by design ([ADR-0003](docs/adr/0003-gfm-extensions.md)). The contents of `content/` are trusted input, exactly like the templates. If you generate a site from untrusted Markdown, sanitise it yourself.
- **Exposing `kappan serve` to a network.** It defaults to `127.0.0.1` and is a development preview server. It is not hardened for public serving; put a real web server in front of the generated files instead.
- Vulnerabilities in dependencies, unless kappan's own usage is what makes them reachable. Report those upstream; tell us too, so we can bump the pinned version.
- Anything requiring an attacker who can already write to your source tree or run code as your user.

## Verifying a release

Every release ships a `SHA256SUMS` covering all four archives.

```bash
sha256sum -c SHA256SUMS --ignore-missing   # macOS: shasum -a 256 -c
```

The binaries are not code-signed or notarised, and the reasoning is recorded in [ADR-0011](docs/adr/0011-release-distribution.md). They are built entirely by [`.github/workflows/release.yml`](.github/workflows/release.yml) on GitHub-hosted runners from the tagged commit — nothing is uploaded from a maintainer's machine.
