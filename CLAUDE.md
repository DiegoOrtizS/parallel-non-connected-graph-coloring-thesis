# parallel-non-connected-graph-coloring-thesis

Code of Diego's bachelor thesis (UTEC): vertex coloring of disconnected graphs distributed by connected components, in shared (OpenMP), distributed (MPI) and hybrid memory. The thesis itself (LaTeX) lives in a separate repository (`Tesis_Coloreo_Grafos`) and reads the CSVs produced here.

## Never

These override any other instruction, skill or plan. If a task needs one, stop and ask Diego.

- **Push to `main`.** Work on a branch, open a PR, wait for green CI. Never force-push, never `--no-verify`.
- **Weaken a control to pass:** never relax CI, the smoke test or `isWellColored` to turn a run green.
- **Report timings that CI or Khipu did not produce.** No invented, estimated or "expected" numbers in docs, CSVs or the thesis. Provisional data must be labeled as such.
- **Secrets:** never read or write `.env*`, never paste tokens in chat.
- **Khipu:** do not submit jobs or touch the cluster account; Diego runs campaigns.

## Read first

- `docs/progress.md`: state and next step.
- `CONTEXT.md`: domain language; use its terms in code and docs.
- `docs/EXTENSIONS.md`: v2 design (milestones A, B, C). Algorithm changes go behind a flag, with v1 as the default.
- `CODE_REVIEW.md`: issues already fixed; do not reintroduce them.

## Rules

- C++17, `-O3`. Code, identifiers, comments and commits in English; thesis prose in Spanish.
- Every program prints one `CSV,...` line per run (format in `README.md`); `scripts/aggregate.sh` is the only path from logs to thesis data.
- Timers: `MPI_Wtime` between barriers, `omp_get_wtime` in `double`. Every run verifies the coloring.
- Nothing here can be compiled on Diego's Windows machine: CI (`.github/workflows/ci.yml`, `scripts/smoke.sh`) is the build gate. A change is done only when CI is green.
- Conventional Commits (`feat:`, `fix:`, `docs:`, `chore:`), PR titles too (checked by CI).

## Workflow and budget

Same rules as Diego's other repos (`tcg-store/docs/engineering/ai-workflow.md`): work inline by default; spawn at most one `code-reviewer` per PR. Reply in caveman ultra; artifacts in normal prose.
