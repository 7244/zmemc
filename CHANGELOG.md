# Changelog

All notable changes to this project will be documented in this file.

## zmemc [0.2.2] - 2025-01-20

### forked from github.com/xeome/Zmem

## [0.2.0] - 2023-12-08

### Bug Fixes

- Fix enqueue function, minor formatting

### Feat

- 35-40% performance improvement by using smaps_rollup instead

### Features

- Root permissions no longer necessary, refactored display functions
- Optimized update function and other various optimizations
- Parallelize process update with Tokio
- Refactor update function and add comments

### ProcessMemoryStats

- :update(): switch to read_line()

### Refactor

- Split src/memory.rs into modules

<!-- generated by git-cliff -->
## [0.1.0] - 2023-12-08

### Bug Fixes

- Fix enqueue function, minor formatting

### Feat

- 35-40% performance improvement by using smaps_rollup instead

### Features

- Root permissions no longer necessary, refactored display functions
- Optimized update function and other various optimizations
- Parallelize process update with Tokio
- Refactor update function and add comments

### ProcessMemoryStats

- :update(): switch to read_line()

### Refactor

- Split src/memory.rs into modules

<!-- generated by git-cliff -->
