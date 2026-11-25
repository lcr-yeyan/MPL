# MPL Type Reference

- `Fragment`: notes/rests (playable).
- `Rhythm`: hit/rest events; not directly playable without pitch.
- `Chord`: `chord("Dm9")`.
- `Harmony`: `harmony span 10 whole { "Dm9" @ 0 whole for 1 whole };`
- `Voicing`: `voicing(chord, low, high, voices, inversion, maxSpacing)`.
- `Pitch`: `C4`, `D#3`, `Bb5`.
- `Duration`: `1/4`, `1 whole`, `1 bars`, `1 beats`.
- `Position`: `0`, `1/4`, `1 bars` in `@` positions (no `whole` after `@`).
- `Int`, `Number`, `Bool`, `String`, `Key`, `[T]` lists.

Context: functions that use `bars`/`beats` or metric hierarchy need
`context meter`. Functions that use `diatonicTranspose`/key need
`context key`.

Examples:

```text
let A = note(C4, 1/4, 96);          // Fragment
let R = rhythm { hit 1/8, rest 1/8 };
let V = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let X = random(7, 0);               // Number in [0,1)
```

Numeric literals are contextually typed. `note(C4, 1/4, 96)` duration is
`Duration`; `@ 1/4` is `Position`; `for 1/4` is `Duration`.
