# MPL Output Protocol

Return exactly one complete MPL program. Do not use Markdown fences, comments
outside the program, prose, or multiple alternatives. The program must start
with `project` (or imports before it) and end with an `export` statement.
Every top-level declaration ends with `;` except block declarations
(`track`, `section`, `arrangement`) whose closing `}` is followed by nothing.
