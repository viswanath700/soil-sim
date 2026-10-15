# SoilSim

SoilSim is a deterministic, engine-independent C++20 core for a simplified
soil-excavation simulation. A bucket applies a constant push force to a
heightfield terrain. The simulation estimates cutting resistance, solves for
penetration depth, removes terrain, tracks loose material in the bucket, and
can dump the material as a spoil mound.

The current implementation covers the core simulation and CLI milestones. It is
a single-scenario runner today; the parallel batch runner is planned as future
work.

## Current capabilities

- Five JSON soil presets:
  `dry_sand`, `wet_clay`, `loam`, `gravel`, and `topsoil`
- SI-unit conversion and physical-parameter validation
- Rankine passive-wedge cutting-force baseline
- Constant-force depth solving with bisection
- Regular-grid heightfield terrain
- Bilinear sampling, finite-difference gradients, and volume calculation
- Rotated square bucket footprint
- Bucket capacity enforcement in loose-volume units
- Bulking-factor material accounting
- Angle-of-repose-limited dumping
- Deterministic heightfield binary serialization
- Scenario and direct command-line execution
- CSV, binary heightfield, benchmark, and force/depth plot outputs
- Catch2 unit tests for the soil, cutting, terrain, conservation, determinism,
  convergence, and binary-file behavior

## Repository layout

```text
core/
  include/soilsim/       Public C++ API
  src/                   Core implementation
tests/                   Catch2 tests
tools/dig_cli.cpp        Single-scenario CLI runner
tools/plot_force.py      Force/depth plot generator
data/soils.json          Soil presets
data/scenarios/          Example scenario and path inputs
docs/model.md            Equations, units, assumptions, and file formats
docs/overall_plan.md     Canonical project plan
docs/benchmarks.md       CLI benchmark log
```

The core library is `soilsim_core`. It has no Unreal Engine dependency or
global simulation state. Unreal integration is optional future viewer work and
is not required to build or test the core.

## Prerequisites

The supported development environment is Windows 11 with:

- Visual Studio 2022 with the C++ workload
- MSVC toolset 14.44 recommended
- Windows 10/11 SDK
- CMake 3.24 or newer
- Ninja
- Python 3
- Python packages `numpy` and `matplotlib` for plotting

The current local validation uses MSVC 14.44, CMake 3.31, Ninja, and the
Debug AddressSanitizer flags configured in [CMakePresets.json](CMakePresets.json).

## Configure, build, and test

Run these commands from a Visual Studio Developer Command Prompt:

```bat
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

The Debug preset uses Ninja, C++20, warnings as errors, debug symbols, and
MSVC AddressSanitizer:

```text
/fsanitize=address /Zi /Od /RTC1
```

Validate the optimized configuration with:

```bat
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Build outputs are written below `out/`, which is intentionally ignored by Git.

## Run the CLI

### Scenario JSON mode

The sample scenario is [data/scenarios/sample.json](data/scenarios/sample.json):

```bat
out\build\release\tools\dig_cli.exe ^
  --scenario data\scenarios\sample.json ^
  --out out\sample
```

The scenario defines one soil, applied force, timestep, flat grid dimensions,
bucket geometry, and a sequence of bucket poses. The sample uses:

- `dry_sand`
- `5000 N` applied force
- `40 x 40` cells
- `0.2 m` cell size
- `1.0 m` initial flat terrain height
- `0.5 m` bucket width
- `10 m^3` loose bucket capacity
- A three-point bucket path

Scenario JSON uses SI units:

| Field | Unit |
|---|---|
| `force_N` | N |
| `dt_s` | s |
| `cell_size_m`, coordinates, heights, widths | m |
| `capacity_m3_loose` | loose m^3 |
| `yaw_rad`, `rake_angle_rad` | radians |

### Direct path mode

Direct mode uses a five-column path file. The sample is
[data/scenarios/sample_path.csv](data/scenarios/sample_path.csv):

```text
x_m,y_m,z_m,yaw_rad,rake_angle_rad
```

Blank lines and lines beginning with `#` are ignored.

```bat
out\build\release\tools\dig_cli.exe ^
  --soil dry_sand ^
  --force 5000 ^
  --path data\scenarios\sample_path.csv ^
  --cell 0.2 ^
  --out out\direct
```

Direct mode currently supplies these defaults because the path-only interface
does not yet define them:

```text
grid:              40 x 40 cells
initial height:    1.0 m
bucket capacity:   10 m^3 loose
bucket width:      0.5 m
timestep:          0.1 s
```

Use scenario JSON mode when these values need to be customized.

## CLI outputs

Each run writes the following files to the selected output directory:

```text
steps.csv
heightfield_final.csv
heightfield_final.bin
```

`steps.csv` contains:

```text
time_s,
depth_m,
force_N,
volume_removed_in_situ_m3,
bucket_fill_m3_loose
```

`heightfield_final.csv` contains one row per heightfield cell:

```text
x_m,y_m,height_m
```

`heightfield_final.bin` is a deterministic little-endian binary heightfield
file. Its header contains the `SOILHF01` magic, version `1`, fixed-width
dimensions, cell size, origin, and row-major IEEE-754 `double` height samples.
The exact format and validation rules are documented in
[docs/model.md](docs/model.md).

The CLI also prints steps per second and appends a result to
[docs/benchmarks.md](docs/benchmarks.md).

## Generate plots

Generate the two force/depth plots:

```bat
python tools\plot_force.py --out docs
```

Optional arguments:

```bat
python tools\plot_force.py ^
  --soils data\soils.json ^
  --force 5000 ^
  --out docs
```

The script writes:

- [docs/force_vs_depth.png](docs/force_vs_depth.png)
- [docs/depth_at_fixed_force.png](docs/depth_at_fixed_force.png)

These images and the benchmark report are intentional project artifacts and
should remain versioned.

## Technical workflow

The current single-scenario workflow is:

```text
CLI arguments or scenario JSON
        |
        v
Load and validate soil preset
        |
        v
Construct a regular heightfield
        |
        v
Create bucket state and Rankine cutting model
        |
        v
For each bucket pose:
  find covered cells
  solve penetration depth from constant force
  lower terrain cells
  convert in-situ removal to loose bucket volume
        |
        v
Write steps and final terrain outputs
        |
        v
Print and record benchmark timing
```

The current CLI constructs a flat heightfield from the scenario grid. The core
API can represent arbitrary per-cell heights, but loading an arbitrary terrain
file through the CLI is not implemented yet.

Likewise, the current CLI runs one soil/force/path combination at a time. To
compare all five presets today, invoke the CLI once per preset. A reusable
terrain-input layer and parallel batch runner are planned future work:

```text
terrain definition + bucket path + soil/force cases
        |
        v
batch runner
        |
        +--> scenario 1
        +--> scenario 2
        +--> scenario 3
        +--> ...
        |
        v
per-case outputs + summary comparison
```

## Model scope and limitations

The current Rankine model is a vertical-blade baseline. It does not yet
model rake-angle effects, soil-tool adhesion, soil-tool friction, moisture,
temperature, freezing, snow, compaction, rocks, layering, or dynamic excavator
mechanics. The heightfield cannot represent overhangs or tunnels.

The current presets are representative placeholders and are marked
`"verified": false`. They are intended for deterministic software validation and
demonstration, not site-calibrated engineering predictions.

See [docs/model.md](docs/model.md) for equations, units, assumptions, material
accounting, convergence criteria, and binary-format details. See
[docs/overall_plan.md](docs/overall_plan.md) for the broader project roadmap.

## Playback viewer

The repository includes an optional Unreal Engine 5.8.3 C++ playback viewer in
[unreal/](unreal/). It reads `heightfield_final.bin` produced by `dig_cli` and
creates a procedural mesh at runtime. Simulation physics remains in
`soilsim_core`; the viewer only loads and displays the result. The viewer
converts the core's metre-valued positions and heights to Unreal centimetres at
the display boundary.

Build the core Release library first:

```bat
cmake --preset release
cmake --build --preset release
```

Then build the Unreal project from a Visual Studio Developer Command Prompt:

```bat
cd unreal
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SoilSimViewer Win64 Development -Project="%CD%\SoilSimViewer.uproject" -WaitMutex
```

Set the actor's `Heightfield File` property to the path of a generated
`heightfield_final.bin`, relative to the Unreal project directory. The
default is `../out/sample/heightfield_final.bin`. The viewer is playback-only;
live linking, weather, terrain editing, and excavator animation are not part
of this integration. The playback actor creates and activates its own camera,
movable directional light, sky atmosphere, and real-time sky light, so a new
level needs only the actor to display the terrain.

The project game mode spawns Unreal's default free-fly pawn. After loading the
terrain, the playback actor positions and aims that pawn at the heightfield.
Use Unreal's default WASD and mouse controls to inspect the scene.
