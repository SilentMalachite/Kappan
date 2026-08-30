# CLI

> 日本語版: [`docs/ja/spec/cli.md`](../ja/spec/cli.md)

## Common

```
kappan --help
kappan --version
kappan --verbose <subcommand>
```

`--version` prints the value of `project(kappan VERSION ...)` in CMake. `--verbose` sets spdlog to debug level.

## `kappan build`

Reads a site root directory, converts the Markdown under `content/` to HTML, copies `static/`, and — when `url` is set — writes `sitemap.xml` and `feed.xml`.

```
kappan build --source <site-root> --out <dir> [--drafts] [--force]
```

- `--source` is the directory containing `site.yaml`. Passing a file prints usage and exits with `ErrorCode::Cli`.
- A missing or malformed `site.yaml` produces `ErrorCode::Config` with a line number.
- A single failing file does not stop the build. Errors are collected and reported at the end; if there is at least one, the exit code is non-zero.
- Output uses pretty URLs (`about.md` → `about/index.html`), rendered with the template that matches `layout`.
- Without `--drafts`, documents with `draft: true` are not written.
- A UTF-8 BOM in the input is dropped and never written to the output. CRLF is normalised to LF.
- `--out` is emptied before writing. If `--out` equals the source root, or the source lives inside `--out`, the command fails with `ErrorCode::Cli`.
- If `--out` is non-empty and its [kappan output marker](output.md#preparing-the-output-directory) is absent or confirmed invalid, the command refuses with `ErrorCode::Cli` **without deleting anything**. Only `--force` allows the deletion.
- If the output-directory state or marker status/content cannot be inspected, the command fails with `ErrorCode::Io` and deletes nothing, as detailed in [output.md](output.md#preparing-the-output-directory). `--force` bypasses marker validation but does not affect the two source-protection checks above — the source is always protected.
- See [output.md](output.md) for details.

## `kappan serve`

Serves the generated site over loopback HTTP. With `--watch`, it observes the source and publishes only generations that built successfully.

```
kappan serve --source <dir> [--host 127.0.0.1] [--port 8080] [--watch] [--drafts]
```

- `--source` is required and must be the directory containing `site.yaml`.
- `--host` defaults to `127.0.0.1`. The server is never exposed to the LAN without an explicit flag.
- `--port` defaults to 8080. The CLI accepts `1..65535`.
- Without `--drafts`, documents with `draft: true` are not served.
- `--watch` observes `site.yaml`, `content/`, `templates/`, and `static/`.
  - Config, Content, and Template changes (including any mix of those with Static) trigger a full rebuild into a new generation.
  - Static-only changes are applied per file and never overwrite a generated page.
  - A failed rebuild does not stop the listener. The error is logged as "which file, what is wrong, and why", and the last known-good generation keeps being served.
  - The generation counter only advances for changes that were applied successfully. The reload script is injected only into HTML responses while watching, so the browser reloads only after a successful generation change — never while a build is failing.
  - `GET /__kappan/reload` returns the generation number as UTF-8 digits with `Cache-Control: no-store`. Without `--watch` it returns 404.
- If the initial site build fails, the server does not listen: it reports the error and exits non-zero.
- If binding fails, it exits non-zero with `ErrorCode::Io` including the host and port.
- `Ctrl-C` (SIGINT, and SIGTERM) stops the listener, joins the HTTP thread, and reclaims the temporary workspace. The signal handler only raises a stop flag — it never calls stop, logging, the filesystem, or a mutex.
- `serve` never creates or modifies `<source>/_site`. Generated files go to the OS temporary directory.

## `kappan new`

```
kappan new <dir>
```

Writes `site.yaml`, a sample article in Japanese, and the bundled theme's `templates/` into an empty directory. A directory that is not empty fails with `ErrorCode::Cli`.

## Exit codes

Expected errors are reported as "which file, what is wrong, and why", with a non-zero exit code.
