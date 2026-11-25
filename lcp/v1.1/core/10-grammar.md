# MPL Grammar Reference (Language 0.3.0)

Top-level order: optional `module Name;`, optional imports, one `project`,
then declarations.

```text
project Name {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset Piano = instrument("GrandPiano");

let M = notes span 1 whole {
  C4 @ 0 for 1/4 vel 96,
  D4 @ 1/4 for 1/4 vel 96
};

track Keys { asset = Piano; role = melodic; }

section S(1 bars) {
  clip Keys "Name" { play M; }
}

arrangement { play S; }
export midi("out.mid");
```

Rules:

- `project` needs `{}` around fields; each field ends with `;`.
- `tempo = 120 bpm;` not `tempo 120`.
- `meter = 4/4;` and `key = C major;` are exact forms.
- `asset Name = instrument("X");` or `sample("X")`.
- `let Name = expression;` ends with `;`.
- `track Name { asset = AssetName; role = melodic; }`.
- Roles: `melodic`, `bass`, `harmony`, `percussion`, `texture`.
- `section Name(N bars) { clip Track "Clip" { play Expression; } }`.
- A clip must have `play <expression>;`.
- `arrangement { play S; }`.
- `export midi("file.mid");`, `export wav("file.wav");`.
- Function: `fn f(x: Int) -> Int { return x; }`; context: `fn f() -> Fragment context meter { ... }`.
- Section meter/grouping: `section S(4 bars) meter 7/8 grouping 2+2+3 { ... }`.
- Frequency: `random(seed, index)`, no recursion.
