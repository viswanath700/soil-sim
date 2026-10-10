# SoilSim — Week 1 Build Plan (for Copilot Agent)

Parent document: `docs/overall_plan.md` (overall project plan). This file covers Week 1 only. If the two conflict, the overall plan's scope and success criteria win; ask before deviating.

## 0. Instructions to the agent
You are scaffolding a C++ project called `soilsim`: a deterministic, engine-free soil excavation simulation core, plus tools and tests. Unreal Engine is an optional viewer task on Day 6.

Target platform: **Windows 11**. Unreal Engine version: **5.8.3**. IDE/toolchain: **Visual Studio 2022** (MSVC toolset 14.44 recommended) with VS Code for editing.

Rules:
- Work day by day. Complete a day's required deliverables and make sure the build and tests pass before moving on. Viewer work is optional and must not displace core validation or documentation.
- Do not add dependencies other than those listed.
- Core library (`core/`) must NOT depend on Unreal, any game engine, or any global state.
- Use SI units everywhere (metres, kilograms, pascals, radians). Use `double` in the core.
- No wall-clock time, no unseeded randomness inside the core. On the same platform, compiler, build configuration, and inputs, repeated runs must produce byte-identical deterministic outputs. Do not promise bit-identical results across compilers or platforms.
- Every equation or assumption you implement must be written to `docs/model.md` with units.
- Use C++20, warnings as errors, and a Debug preset with AddressSanitizer (MSVC `/fsanitize=address`; UBSan is not available on MSVC).
- The core and the Unreal module must be built with the same MSVC toolset.
- Prefer small commits: one per day minimum.

## 1. Goal for Week 1
A C++ core that digs a heightfield with a documented, physically grounded cutting-force baseline, with unit tests, a CLI runner, and force-vs-depth plots. A minimal Unreal playback viewer is optional and must not delay the core deliverables.

Core idea: a bucket applies a **constant push force**; the sim finds the penetration depth where soil resistance equals the applied force. Strong soil gives a shallow cut; weak soil gives a deep cut.

Week 1 maps to the overall plan as: core foundation, soil presets, Rankine cutting model, constant-force solver, heightfield dig and dump, CLI, plots, core tests, minimal Unreal playback.

## 2. Tooling (Windows 11, UE 5.8.3)
- VS Code extensions: C/C++ (or clangd), CMake Tools, CMake Test Explorer, clang-format.
- Visual Studio 2022 (Community is fine) with the "Game development with C++" workload, C++ AddressSanitizer, C++ profiling tools, and Windows 10/11 SDK 10.0.18362 or newer. Stay on VS 2022 for now; VS 2026 has reported problems with UE 5.8.x.
- MSVC toolset: Epic recommends 14.44 for VS 2022 with UE 5.8. Verify against `Engine\Config\Windows\Windows_SDK.json` in the UE install. If Unreal builds fail with compiler errors, install the exact 14.44 toolset side by side.
- In the UE editor: Preferences, Source Code, select Visual Studio 2022.
- Run CMake builds from a Developer Command Prompt (or use CMake presets that load the MSVC environment) with Ninja and `cl.exe`.
- CMake >= 3.24, Ninja, Git, Python 3 (numpy, matplotlib).
- Test framework: Catch2 via CMake `FetchContent`.
- JSON: nlohmann/json via `FetchContent` (used only in tools and data loading).

UE toolchain verification is a prerequisite only if the optional viewer is pursued this week. It must not block the core build if Unreal is unavailable.

## 3. Repository layout to create
```
soilsim/
  CMakeLists.txt
  CMakePresets.json
  .clang-format
  .gitignore
  README.md
  core/
    CMakeLists.txt
    include/soilsim/
      types.h
      soil.h
      heightfield.h
      cutting.h
      excavate.h
    src/
      soil.cpp
      heightfield.cpp
      cutting.cpp
      excavate.cpp
  tests/
    CMakeLists.txt
    test_soil.cpp
    test_cutting.cpp
    test_heightfield.cpp
    test_excavate.cpp
  tools/
    CMakeLists.txt
    dig_cli.cpp
    plot_force.py
  data/
    soils.json
    scenarios/            (one example scenario JSON; more in later weeks)
  docs/
    overall_plan.md       (canonical overall plan)
    model.md
    limitations.md
    benchmarks.md
  .vscode/
    tasks.json
    launch.json
    settings.json
  unreal/                 (created on Day 6)
```
Not created this week: batch runner (Week 3), environment state logic (Week 2).

## 4. Day-by-day deliverables

### Day 1 — Repo, build, test harness, toolchain check
- First verify that CMake, Ninja, and the selected MSVC compiler are available; report the installed MSVC toolset version and Windows SDK. If UE is a target for this week, separately report whether its recommended toolchain is installed.
- Establish the smallest working CMake configure/build/test path first: root and core CMake files, configure/build/test presets named `debug` and `release` in `CMakePresets.json` (Ninja generator; MSVC `cl.exe`; debug preset adds `/fsanitize=address` and `/Zi`), and one trivial test.
- Once that path works, add the remaining layout, tools/test CMake files, `.clang-format`, `.gitignore`, and `.vscode` tasks for configure, build, and test.
- `core` is a static library `soilsim_core`. `tests` builds `soilsim_tests` with Catch2. `tools` builds `dig_cli`.
- Keep `docs/overall_plan.md` as the single canonical overall plan; do not create a duplicate copy.
- If pursuing the optional viewer, remind the user to create and compile a blank UE 5.8.3 C++ project to confirm the Unreal toolchain before Day 6. Otherwise defer this check.
- Done when: `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug` all pass from a clean checkout.

### Day 2 — Soil model
`core/include/soilsim/soil.h`:
```cpp
struct SoilParams {
  std::string name;
  double cohesion;          // Pa
  double frictionAngle;     // rad
  double unitWeight;        // N/m^3
  double adhesion;          // Pa (soil-tool)
  double soilToolFriction;  // rad (delta)
  double bekkerKc, bekkerKphi, bekkerN; // used from week 2
  double angleOfRepose;     // rad
  double bulkingFactor;     // swell factor when excavated (about 1.1-1.4)
};
```
- `data/soils.json`: presets `dry_sand`, `wet_clay`, `loam`, `gravel`, `topsoil`. Store values in friendly units (kPa, degrees) in JSON; convert to SI on load. Add `"source"` and `"verified"` fields per preset (cite a terramechanics table; placeholder values are acceptable but must be flagged `"verified": false`).
- Loader function `loadSoils(path)` with validation (positive unit weight, nonnegative cohesion and adhesion, angles between 0 and 90 degrees, positive bulking factor, and valid/nonnegative Bekker parameters where supplied) and clear error messages.
- Tests: JSON load, unit conversion, validation failures.

### Day 3 — Cutting force and constant-force solver
`core/include/soilsim/cutting.h`:
```cpp
struct CuttingResult { double force; double forceHorizontal; double forceVertical; };

class ICuttingModel {
 public:
  virtual CuttingResult cut(const SoilParams&, double depth, double width,
                            double rakeAngle, double surchargePa) const = 0;
  virtual ~ICuttingModel() = default;
};

class RankineCuttingModel final : public ICuttingModel { /* v1 */ };

// Finds depth where resistance == appliedForce. Bisection over [0, maxDepth];
// Rankine v1 resistance must be monotonic over the validated input domain.
double solveDepthForForce(const ICuttingModel&, const SoilParams&, double appliedForce,
                          double width, double rakeAngle, double surchargePa,
                          double maxDepth, double tol = 1e-6);
```
Version 1 force (vertical blade of width `w`, depth `d`, surcharge `q`):

F = w * ( 0.5 * gamma * d^2 * Kp + 2 * c * d * sqrt(Kp) + q * d * Kp ),  Kp = tan^2(45deg + phi/2)

- This baseline assumes a vertical blade; document the rake-angle convention and that the Week 1 Rankine implementation does not model rake angle, adhesion, or soil-tool friction. Keep the interface so Reece's fundamental earthmoving equation (depth, blade angle, density, friction, cohesion, adhesion terms) can replace it in Week 2 without touching callers. Add `// TODO(week2): ReeceCuttingModel` in the header.
- The `surchargePa` argument exists now so snow load and overlying soil can use it in Week 2.
- Validate finite inputs and physical ranges; reject invalid inputs with a clear error rather than returning a plausible-looking result.
- Tests (closed-form checks): zero-cohesion sand gives force proportional to d^2; phi = 0 gives Kp = 1 and matches a hand calculation; force is monotonic in depth, friction angle, and cohesion over supported ranges; solver converges for every preset and brackets a solution on [0, maxDepth]. Return 0 for nonpositive applied force; return `maxDepth` when applied force exceeds resistance at `maxDepth`. Document that this is a capped-depth result, not an equality solution.

### Day 4 — Heightfield and dig step
`heightfield.h`: regular grid, flat `std::vector<double>`, cell size, origin; bilinear sampling; gradient; volume above datum; per-cell arrays for future state (moisture, frozenFraction, compaction) declared but unused until Week 2.

`excavate.h`:
```cpp
struct BucketPose { double x, y, z, yaw, rakeAngle, width; };
struct DigStepResult { double time, depth, force, volumeRemoved, bucketFill; };
struct BucketState { double capacity; double carriedVolume; };

DigStepResult digStep(Heightfield&, BucketState&, const SoilParams&, const ICuttingModel&,
                      const BucketPose&, double appliedForce, double dt);
void dump(Heightfield&, BucketState&, const SoilParams&, double x, double y);
```
- Dig step: find cells under the bucket footprint, solve depth with the constant-force rule, clamp to the commanded path and remaining bucket capacity, lower cells, accumulate removed in-situ volume, convert it to loose volume using `bulkingFactor`, and add it to the bucket (respect capacity in loose-volume units). If the bucket fills before all soil can be removed, leave uncollected soil in the terrain; do not silently discard material.
- Dump: add a cone-shaped mound limited by the angle of repose (full slump relaxation comes in Week 3).
- For this uniform-soil, no-moisture baseline, account for material using in-situ-equivalent volume: terrain volume plus `(bucket loose volume + spoil loose volume) / bulkingFactor`. This is equivalent to mass balance while unit weight and soil composition remain unchanged; document the accounting and tolerance in `docs/model.md`.
- Tests: a closed dig-and-dump scenario must have relative material-balance error <= 1e-6; bucket capacity is respected in loose-volume units; deterministic repeated runs compare exact serialized output bytes on the same build/platform, including final heightfield and bucket state.
- Convergence test: use one fixed, documented flat-terrain scenario, compare total in-situ dig volume at cell sizes `0.20 m` and `0.10 m`, and require relative difference <= 5%. Treat this as a regression threshold for that scenario, not a universal accuracy guarantee.

### Day 5 — CLI, outputs, plots, benchmark
- `dig_cli` args: `--soil NAME --force N --path FILE --cell SIZE --out DIR`, and `--scenario FILE` to load `data/scenarios/*.json` (soil, force, bucket path, grid size, and bucket geometry). Define the scenario schema and units in a sample JSON and `README.md`; path input must have a documented format. Writes `steps.csv` (time, depth, force, in-situ volume removed, loose bucket fill), `heightfield_final.bin`, and a heightfield CSV export. Keep the interface argument-driven so the Week 3 batch runner can wrap it.
- Define the binary heightfield format in `docs/model.md`: magic/version, fixed-width dimensions, cell size and origin as IEEE-754 doubles, followed by row-major height samples as IEEE-754 doubles. Specify byte order (little-endian), datum convention, and that Week 1 output contains heights only. Reject unsupported versions and malformed/truncated files in the reader.
- `tools/plot_force.py`: (1) force vs depth for every soil; (2) depth reached at a fixed applied force per soil. Save PNGs to `docs/`.
- Benchmark: `std::chrono` timing of dig steps per second; print and append to `docs/benchmarks.md`.

### Day 6 — Minimal Unreal viewer (optional)
- Create `unreal/` as a UE 5.8.3 C++ project. Build `soilsim_core` as a static library (Release, `/MD` runtime to match Unreal) with the same MSVC toolset as the Unreal project, and integrate it as a ThirdParty module. Convert `double` to `float` only at the Unreal boundary.
- Start with file playback: load files written by `dig_cli` before attempting live linking (live link is Week 3).
- Playback only: read the documented final-heightfield format and apply it to a procedural mesh or displacement texture on a plane. A placeholder excavator (primitives) follows the bucket path. If frame playback is needed, defer it until the CLI emits a documented frame format.
- Do NOT put physics inside Unreal. No weather or Landscape deformation yet.
- Fallback: if the C++ Unreal project fails to build, use a Blueprint-only project that reads the CSV and heightfield files.

### Day 7 — Review and buffer
- Reserve this day for build/test regressions, documenting the equations actually implemented, and locking the CLI and binary output formats. Do not add new features.
- Update `docs/model.md` (equations, units, assumptions, material accounting, formats, known limitations), update `README.md` with build and run steps, and tag `week1` only after required deliverables pass.
- Write `docs/limitations.md`: heightfield cannot represent overhangs; Rankine wedge is a quasi-static 2D simplification; no rocks or layered soil; no rate-dependence; parameters are representative, not site-calibrated.

## 5. Definition of done for Week 1
### Required: core and tools
- Clean build and all tests pass on the Debug (ASan) and Release presets with MSVC on Windows 11.
- `dig_cli` produces CSV and heightfield outputs for all five soil presets.
- Two plots exist in `docs/`.
- Determinism and material-balance tests pass at the documented thresholds; the fixed-scenario convergence test passes at the documented cell sizes and tolerance.
- `docs/model.md` documents every implemented equation, units, assumptions, material accounting, and binary format; `docs/limitations.md` and `docs/benchmarks.md` exist.
- Under one documented fixed force, path, and bucket setup, the five presets produce a max-to-min depth ratio >= 1.20 and a max-to-min excavated-volume ratio >= 1.10. Report the scenario and values; if the thresholds are not met, investigate the force model and preset parameters rather than silently changing test expectations. These are demonstrator thresholds, not claims of site-calibrated accuracy.
### Optional: viewer
- If time and toolchain permit, the Unreal project builds and reads the documented output format. Viewer work does not block Week 1 completion.

## 6. Out of scope for Week 1 (do not build yet)
- Week 2: moisture, temperature, freeze-thaw, snow, effective-strength function, Reece model, sensitivity sweeps, weather materials.
- Week 3: slump relaxation, compaction, batch runner, live Unreal link, particles, calibration experiment.
- Week 4: scenario presets UI, A/B view, time-of-day and season controls, fallback video.
- UE 5.8 Mesh Terrain (experimental true-3D-mesh terrain): do not depend on it; future work.
- GPU or SIMD paths, vegetation, groundwater.

## 7. First task for the agent
Start with Day 1. First report the installed MSVC toolset version and Windows SDK. Then establish the minimal CMake configure/build/test path and placeholder test; after it passes, create the remaining Week 1 layout with compiling stubs and wire the remaining targets. Confirm `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug` succeed. If the required compiler or SDK is missing, report the exact blocker and do not silently switch toolchains. Then stop and report before starting Day 2.
