## What this changes

<!-- One or two sentences. What the diff does is visible; say why it is needed. -->

## Related issue

<!-- Closes #123, or "none" with a note on why this did not need discussion first. -->

## Checks

- [ ] `cmake --build --preset dev` passes with zero warnings
- [ ] `ctest --preset dev --output-on-failure` passes completely
- [ ] New or changed behaviour has a test
- [ ] Verified against input containing Japanese (kana/kanji), emoji, or mixed halfwidth/fullwidth text
- [ ] One commit, one purpose — no unrelated formatting or renames
- [ ] Docs updated: `docs/spec/` for behaviour changes, `docs/adr/` for design decisions

<!--
If you changed a public header under include/kappan/, an ADR is required.
If you added a dependency, say so here — it needs agreement before merge.
See CONTRIBUTING.md for the full details.
-->
