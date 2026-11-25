# MPL Diagnostic Guide

Common first errors and exact fixes:

| Diagnostic | Fix |
| --- | --- |
| `parse-expected-equal` after `tempo` | write `tempo = 120 bpm;` |
| `parse-expected-lbrace` after `project` | use `project Name { ... }`; do not quote the project name |
| `parse-expected-semicolon` | add `;` after each field/let/asset/export |
| `parse-expected-lparen` after section name | write `section S(1 bars) { ... }` |
| `parse-unexpected-clip-item` | clip body must be `play <expr>;` (not `notes { ... }`) |
| `parse-missing-play` | add `play M;` inside the clip |
| `parse-unexpected-project-field` | fields are only `tempo`, `meter`, `key` |
| `type-mismatch` | check expected/actual; `@` needs Position, `for` needs Duration |
| `sema-unknown-symbol` | declarations must precede use; check spelling |
| `sema-missing-context` | add `context meter`/`context key` |
| `sema-section-overflow` | clip content longer than its section |
| `limit-call-depth` | reduce nested function calls (max 32) |

Do not invent syntax such as `C4 at 0 duration 1/4 velocity 96`,
`track "Keys" role melodic { ... }`, `clip Keys { notes { ... } }`,
`section S { bars 1 }`, or `project "Basic"`. These are not MPL.
