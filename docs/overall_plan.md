# SoilSim — Overall Project Plan

## 1. Overview
SoilSim is a real-time, semi-empirical soil and terrain simulation for excavation. A constant-force excavator bucket digs into terrain whose behavior changes with soil type, moisture, temperature, snow, and season. The result is a deterministic C++ simulation core, a headless batch runner, and an Unreal Engine 5.8.3 viewer.

**Platform:** Windows 11, Visual Studio 2022 (MSVC toolset 14.44 recommended), CMake + Ninja, Unreal Engine 5.8.3.
**Duration:** 4 weeks.

## 2. Objectives
- Show how the same excavator input produces different dig results under different ground conditions.
- Keep the physics in an engine-independent, deterministic, testable C++ library.
- Run many scenarios headless and fast, with measurable throughput.
- Use physically grounded models with documented equations, units, assumptions, and limitations.
- Use Unreal only for visualization, weather, and interactive scenario control.

## 3. Scope

### In scope
**Soil and strength model**
- Soil presets: dry sand, wet clay, loam, gravel, topsoil (parameters stored in data files with sources).
- Mohr-Coulomb strength (cohesion, friction angle, unit weight).
- Cutting force: Rankine passive wedge (v1), upgraded to Reece's fundamental earthmoving equation (rake angle, adhesion, soil-tool friction).
- Constant-force dig solver: find depth where soil resistance equals applied force.

**Environmental factors**
- Moisture: rain infiltration, drying, saturation; apparent cohesion that rises then falls with moisture.
- Temperature: freezing, frozen crust, thaw softening.
- Snow cover as a surcharge and insulating layer; melt.
- Time of day and season driving temperature and evaporation.
- Compaction from repeated passes; sinkage (Bekker pressure-sinkage).

**Terrain behavior**
- Heightfield terrain with per-cell state (moisture, frozen fraction, compaction, layers).
- Excavation, bucket fill with bulking factor, and dumping.
- Slumping and angle-of-repose relaxation; spoil piles.
- Slope and drainage effects (basic runoff).

**Tooling and quality**
- CLI scenario runner and parallel batch runner (no rendering).
- CSV/binary outputs, plots, and telemetry.
- Unit, conservation, determinism, convergence, and sensitivity tests.
- Benchmarks (steps per second, runs per second, scaling by core count).

**Visualization (Unreal Engine 5.8.3)**
- Playback of simulation output, then live link to the core.
- Terrain material driven by state (wet/dark, mud, frost, snow, dust).
- Niagara effects (dust, clods, splash).
- Rigged excavator following the bucket path.
- Scenario presets and A/B (side-by-side) comparison with on-screen telemetry (force, depth, volume, bucket fill, moisture).

### Out of scope
- Full 3D granular or particle physics solvers and research-grade validation.
- Excavator hydraulics and dynamics beyond a driven arm.
- Large-scale world building and photoreal environment art.
- Vegetation and groundwater modeling (cut first if behind).
- Multiplayer, packaging for other platforms.

## 4. Architecture
- **core/** — engine-free C++20 library: soil model, cutting model, heightfield, environment state, excavation, slump. SI units, `double`, deterministic, no globals.
- **tools/** — `dig_cli` (single scenario), batch runner (many scenarios in parallel), plot scripts (Python).
- **data/** — soil presets and scenario definitions (JSON).
- **tests/** — Catch2 tests.
- **unreal/** — UE 5.8.3 project; `soilsim_core` linked as a ThirdParty static library; physics never lives in Unreal.
- **docs/** — model equations, assumptions, limitations, benchmarks.

## 5. Weekly Targets

| Week | Target |
|---|---|
| 1 | Core foundation: repo and build, soil presets, Rankine cutting model, constant-force solver, heightfield dig and dump, CLI, force/depth plots, core tests, minimal Unreal playback viewer. |
| 2 | Environment model: moisture, temperature, freezing, snow; effective-strength function; Reece cutting model; sensitivity sweeps; weather visuals and terrain material in Unreal. |
| 3 | Terrain behavior and scale: slumping, spoil, compaction; particles and telemetry in Unreal; headless parallel batch runner; convergence and determinism tests; parameter calibration experiment; throughput benchmarks. |
| 4 | Integration and polish: scenario presets, A/B comparison view, time-of-day and season controls, fallback recorded video, documentation, final profiling, bug fixing. |

Cut order if behind: layering, vegetation, groundwater, erosion. Protect: core tests, constant-force A/B comparison, weather-driven strength changes, headless throughput numbers.

## 6. Deliverables
1. **Simulation core library** with documented models and passing tests.
2. **Soil and scenario data files** with cited sources and verification status.
3. **CLI and batch runner** producing CSV, heightfield outputs, and summary metrics.
4. **Plots and reports:** force vs depth per soil, depth at fixed force, sensitivity ranking, convergence results, throughput benchmarks.
5. **Unreal Engine project** with terrain playback/live view, weather and state-driven materials, excavator, effects, and telemetry overlay.
6. **Scenario set:** dry sand, wet clay, frozen ground, snow-covered soil, plus rain and season variations, each under the same bucket force.
7. **Documentation:** `docs/model.md` (equations, units, assumptions), `docs/limitations.md`, build and run instructions, benchmark log.
8. **Recorded demo video** as a fallback to the live build.

## 7. Success Criteria
- Same applied force gives clearly different, physically plausible dig depth and volume across soil and weather conditions.
- Results are bit-identical across repeated runs with identical inputs.
- Dig volume converges as cell size and timestep are refined.
- Volume is conserved through dig, carry, and dump.
- Headless batch throughput is measured and reported.
- A parameter-fitting experiment recovers known parameters from synthetic data within a stated error.

## 8. Risks and Mitigations
| Risk | Mitigation |
|---|---|
| Unreal and MSVC toolchain mismatch | Stay on VS 2022 with the recommended toolset; compile a blank UE C++ project on Day 1; same toolset for core and Unreal module. |
| Viewer work consumes physics time | Playback-only viewer first; no physics in Unreal. |
| Parameter values unverified | Flag presets as unverified; cite sources; sensitivity analysis shows which parameters matter. |
| Model too simple to look realistic | Layer visuals (materials, particles) over the model; state limitations openly. |
| Live demo failure | Recorded fallback video and prebuilt packaged build. |

## 9. Known Limitations
- Heightfield cannot represent overhangs, undercuts, or tunnels.
- Rankine and Reece models are quasi-static, 2D wedge simplifications.
- No rate-dependent or dynamic soil flow; no rocks or heterogeneous layers in the base version.
- Parameters are representative, not site-calibrated.

## 10. Future Considerations
- **Hybrid particles near the bucket** for realistic flow and clumping, with the heightfield for the far field.
- **True 3D terrain:** voxel/SDF or UE 5.8 Mesh Terrain (experimental) to support overhangs, caves, and undercuts.
- **Continuum methods:** SPH or material point method as high-fidelity references for validating the fast model.
- **Learned residual correction** fitted to measured force or hydraulic data on top of the physics model.
- **Calibration pipeline:** fit parameters from real digging logs; domain randomization over parameter uncertainty.
- **GPU acceleration** (compute shaders or CUDA) for state updates, slumping, and large batch runs; SIMD on CPU.
- **Layered and heterogeneous soils:** strata, rocks, buried objects, roots.
- **Richer hydrology:** groundwater table, erosion rills, ponding, drainage design.
- **Machine dynamics:** hydraulic actuator models, track/wheel terramechanics, tip-over and stability.
- **Sensor simulation:** depth cameras, LiDAR on deformable terrain.
- **Scene generation and neural rendering:** Gaussian splatting or NeRF-based site backgrounds combined with physics-driven terrain.
- **Interfaces for control and ML:** Python bindings, gym-style API, scenario randomization and result dashboards.
- **Other tooling:** continuous integration with regression benchmarks; Rust or other language bindings if useful.
