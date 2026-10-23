# Tool organization

- [`analysis/`](analysis/): audits and loss analyses.
- [`data-preparation/`](data-preparation/): PGN and cohort preparation.
- [`reproduction/`](reproduction/): reproduce historical experiments.
- [`reporting/`](reporting/): speed and quality summaries.
- [`validation/`](validation/): checks and validation.

Original entry-point filenames remain at `tools/` as lightweight launchers for backward compatibility. Research C++ and header files, `build_*`, `generate_*`, and `benchmark_*` scripts remain at original paths because they have many direct CMake, relative include, or script references. Their future relocation requires an end-to-end build and test pass.
