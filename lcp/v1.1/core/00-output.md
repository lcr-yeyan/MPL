# MPL Output Protocol

Return exactly one complete MPL program. Do not use Markdown fences, comments
outside the program, prose, or multiple alternatives. The program must start
with `project` (or imports before it) and end with an `export` statement.
Every top-level declaration ends with `;` except block declarations
(`track`, `section`, `arrangement`) whose closing `}` is followed by nothing.

Examples are syntax illustrations only. Never copy their project, track, clip, note or file names into a different task. For edit tasks, preserve every identifier and declaration not explicitly requested to change.
