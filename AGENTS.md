# Project Brain Protocol

The repository-local `obsidian-markdown` skill is installed at `.agents/skills/obsidian-markdown/`. When creating or editing project-brain notes, read its `SKILL.md` and use Obsidian-compatible Markdown.

## Start here

1. Read `docs/INDEX.md` first.
2. Treat `docs/PRODUCT_SPEC_V2.md` and `docs/DECISIONS.md` as the product source of truth.
3. Treat `docs/CURRENT_STATE.md` as the source of truth for what is actually implemented now.
4. Never assume a planned feature exists merely because it appears in the product spec.
5. Follow links from `docs/INDEX.md` and read only the notes relevant to the current task; do not load the entire project brain by default.

## Keep the brain accurate

- Update `docs/CURRENT_STATE.md` only after a feature is implemented and verified. Record the verification performed.
- Update `docs/DECISIONS.md` whenever a product decision is added, changed, or rejected.
- Put postponed or exploratory ideas in `docs/FUTURE_IDEAS.md` instead of expanding current scope.
- Keep notes concise, prefer wikilinks for internal references, and avoid duplicating detail across notes.
- Never resurrect a rejected feature without an explicit new decision recorded in `docs/DECISIONS.md`.

## Engineering guardrails

- Preserve realtime-audio safety and existing working functionality.
- Keep decoding, allocation, blocking, file I/O, cache preparation, and expensive analysis off the realtime audio thread.
- Do not redesign the VST, audio/DSP behavior, or build setup unless the current task explicitly requires it.
- Inspect the implementation and run proportionate verification before claiming a feature works.

