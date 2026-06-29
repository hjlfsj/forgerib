# forgerib

This repository contains a CERN ROOT based data-processing pipeline for the RIBLL2026 experiment. It turns raw XIA and VME data into detector-specific ROOT outputs, aligns XIA and VME triggers, and produces forged trigger/event files for downstream analysis.

> AI-generated notice: this README was generated with AI assistance from the repository contents and should be reviewed like source code documentation.

## Purpose

At a high level, the project does the following:

- Reads raw XIA detector data and raw VME ROOT files.
- Splits and converts them into detector-level ROOT files.
- Aligns XIA and VME trigger timing.
- Builds trigger products and detector event products for analysis.

The main processing stages reflected by the code and scripts are:

- `crush`: convert raw XIA data into detector-specific ROOT files.
- `crush_vme`: split mapped VME ROOT input into detector-specific ROOT files.
- `sift`: align XIA and VME trigger entries.
- `coke`: forge trigger outputs from aligned trigger data.
- `smelt`: build final detector event trees in the `ingot` output area.

## Requirements

- CMake 3.10 or newer
- A C++17 compiler
- CERN ROOT 6.20 or newer, available through `find_package(ROOT ... CONFIG REQUIRED)`

This project requires an out-of-source build.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

The main executables are created under `build/bin/`, including:

- `build/bin/crush/crush_*`
- `build/bin/crush_vme`
- `build/bin/sift`
- `build/bin/coke`
- `build/bin/smelt`

## Configuration

Runtime configuration is read from [`config.toml`](/home/pupu/code/forgerib/config.toml) by default. Most command-line tools in this pipeline also accept `-c, --config <path>` to override it. The important keys are:

- `workspace`: base directory for generated outputs.
- `trigger`: default trigger type.
- `paths.raw_xia0`, `paths.raw_xia1`, `paths.raw_xia2`: raw XIA input directories.
- `paths.raw_vme`: raw VME ROOT input directory.
- `paths.ore`, `paths.grit`, `paths.grain`, `paths.ingot`: output subdirectories under `workspace`.
- `prefix.raw_xia0`, `prefix.raw_xia1`, `prefix.raw_xia2`, `prefix.raw_vme`: file-name prefixes for input files.

The current layout implied by the code is:

- `ore`: intermediate decoded/mapped data.
- `grit`: crushed detector-level files.
- `grain`: XIA/VME alignment output.
- `ingot`: final forged trigger and detector event outputs.

Make sure these directories and input files exist before running the pipeline.

## How To Use

The fastest way to run the standard pipeline is the helper script [`scripts/forge.sh`](/home/pupu/code/forgerib/scripts/forge.sh). Run it from the repository root and pass a run number:

```bash
bash scripts/forge.sh 123
```

`scripts/forge.sh` currently executes:

```bash
./build/bin/crush/crush_ppac 123
./build/bin/crush/crush_beam 123
./build/bin/crush/crush_trigger 123
./build/bin/crush/crush_t0d1 123
./build/bin/crush/crush_t0d2 123
./build/bin/crush/crush_t0d3 123
./build/bin/crush/crush_t0d4 123
./build/bin/crush/crush_others 123
./build/bin/crush_vme 123
./build/bin/sift -x 123 -v 123
./build/bin/coke -r 123
./build/bin/smelt -r 123 -v 123 -t t1 t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac tafd tafcsi t1csiu t1csid t1du t1dd t1sd t1su beam
./build/bin/smelt -r 123 -v 123 -t main t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac beam
```

This means the script assumes:

- the project has already been built,
- you are running from the repository root,
- XIA and VME data for the same run number should be processed together.

If XIA and VME run numbers differ, run the binaries manually instead of using `scripts/forge.sh`.

## Manual Commands

Useful direct invocations:

```bash
./build/bin/crush/crush_trigger 123
./build/bin/crush_vme 123
./build/bin/sift -x 123 -v 123
./build/bin/coke -r 123
./build/bin/smelt -r 123 -v 123 -t t1 t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac tafd tafcsi t1csiu t1csid t1du t1dd t1sd t1su beam
./build/bin/smelt -r 123 -v 123 -t main t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac beam
```

You can also inspect command-line help for the tools that use `cxxopts`:

```bash
./build/bin/sift --help
./build/bin/coke --help
./build/bin/smelt --help
./build/bin/crush_vme --help
```

Examples with an alternate config file:

```bash
./build/bin/crush/crush_trigger -c my-config.toml 123
./build/bin/crush_vme -c my-config.toml 123
./build/bin/sift -c my-config.toml -x 123 -v 123
./build/bin/coke -c my-config.toml -r 123
```

## Notes

- `scripts/crush.sh` runs only the XIA `crush_*` stage.
- `bin/mine.cpp` and `bin/tune/` provide auxiliary tooling and are not part of the standard `forge.sh` flow.
