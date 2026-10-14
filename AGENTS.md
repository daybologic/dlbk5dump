# Repository Guidelines

## Project Structure & Module Organization

This repository contains a small C++ reverse-engineering utility for BK5 family-history data. Application entry code is in `src/dlbk5dump.cpp`; marriage-record parsing is split between `src/marriage.cpp` and `src/marriage.hpp`. The `BK5/` directory holds the legacy database and program files used as runtime input, while `bk5.iso` is the source disk image. `README` contains the project’s brief runtime requirement. There is currently no separate test, asset, or generated-output directory.

## Build, Test, and Development Commands

There is no checked-in build system. From the repository root, compile the utility with:

```sh
g++ -std=c++17 -Wall -Wextra src/dlbk5dump.cpp src/marriage.cpp -o dlbk5dump
```

Run it from the repository root so its relative `BK5/...` paths resolve correctly:

```sh
./dlbk5dump
```

The program checks for required `.DT5` files and then reads the first three marriage records. No automated test suite is currently provided; use the bundled BK5 data for a smoke test and inspect the exit status and printed records.

## Coding Style & Naming Conventions

Match the existing C++ style: tabs for indentation, braces on the same line, and explicit standard-library headers. Keep declarations in headers and implementations in corresponding `.cpp` files. Use descriptive PascalCase for existing types and methods (for example, `TReadMarriages::Load`) and preserve the existing uppercase field naming in legacy data structures. Avoid changing record offsets or binary layouts without documenting the format assumptions.

## Testing Guidelines

Before submitting changes, rebuild with warnings enabled and run `./dlbk5dump` from the root. Confirm missing-input behavior by running in a directory without `BK5/` only when needed; do not alter or delete the supplied legacy data. If parser behavior changes, include representative output and explain which BK5 record layout was validated.

## Commit & Pull Request Guidelines

Use short, imperative commit subjects, consistent with the existing history (for example, `Recover files from Hg at ...`). Keep commits focused. Pull requests should describe the parser or data-format change, identify any assumptions about BK5 files, include the commands used for validation, and attach relevant sample output when behavior changes. Do not commit generated binaries or local BK5 data unless explicitly required.

## Data and Repository Hygiene

Treat `BK5/` and `bk5.iso` as input fixtures, not disposable build artifacts. Work from the repository root, avoid modifying legacy files during experiments, and keep compiled executables out of commits.
