# Limitations

SoilSim is a deterministic software demonstrator for excavation behavior. It is
not a site-calibrated engineering tool. Do not use its results for design,
safety, productivity, or cost decisions.

## Cutting model

- The Rankine passive-wedge equation is a quasi-static, two-dimensional
  simplification. It treats the bucket as a vertical blade of fixed width.
- Rake angle, soil-tool adhesion, and soil-tool friction are not modeled.
- Resistance is evaluated per cell as a static balance. Cutting speed, inertia,
  and rate-dependent soil behavior are ignored.
- Soil is homogeneous and isotropic. Layering, rocks, roots, and buried objects
  are not represented.
- The Bekker parameters are stored and validated but are not used by the current
  cutting force.
- Failure geometry is an assumption, not a result: there is no shear-band
  development, crack propagation, or progressive failure.

## Soil and environment

- Preset values are representative placeholders and are marked
  `"verified": false` in [data/soils.json](../data/soils.json). They are not
  site-calibrated and have not been checked against laboratory or field data.
- Soil properties are constant. Moisture, temperature, freeze-thaw, snow, and
  weather do not change strength.
- Groundwater, pore pressure, and drainage are not modeled.
- Compaction, density change from loading, and consolidation are not modeled.

## Terrain representation

- The terrain is a regular-grid heightfield with one height per cell. It cannot
  represent overhangs, tunnels, or undercuts.
- Excavation lowers cell heights. Bilinear sampling and finite-difference
  gradients make results depend on cell size; accuracy is established only by
  the documented convergence test, not for arbitrary resolutions.
- The bucket footprint is a rotated square. Round or shaped buckets are not
  modeled.
- Spoil is placed as a cone at the angle of repose. Slumping, flow, and
  relaxation of unstable slopes are not modeled.
- Material accounting uses a single bulking factor per soil. Swell and
  recompaction change only the loose-volume bookkeeping.

## Excavator mechanics

- A constant applied force drives penetration. Hydraulics, linkage kinematics,
  engine power, traction, stability, and operator behavior are not modeled.
- The bucket follows a prescribed path. Path feasibility and reach limits are
  not checked.
- The CLI constructs a flat terrain from the scenario grid. Loading arbitrary
  terrain files is not implemented.
- The CLI runs one soil, force, and path combination per invocation. There is no
  parallel batch runner yet.

## Determinism and numerics

- The core uses `double` precision with no wall-clock time and no randomness.
  Repeated runs on the same platform, compiler, build configuration, and inputs
  are expected to produce identical outputs.
- Bit-identical results across compilers, platforms, or optimization settings
  are not guaranteed.
- The binary heightfield format assumes a little-endian host.
- Depth solving uses bisection with a finite iteration count, so reported
  depths are approximate to the solver's resolution.

## Unreal viewer

- The viewer is playback-only. It displays the final heightfield and runs no
  physics.
- It shows only the final terrain state. Per-step playback, excavator and
  bucket visualization, particles, and telemetry are not implemented.
- The terrain mesh has no dedicated material, so appearance is not driven by
  soil or weather state.
- The viewer links the Release `soilsim_core` library, so build that
  configuration before building the Unreal project.
- Core metres are converted to Unreal centimetres at the display boundary. The
  rendered scale is not otherwise validated against real-world dimensions.
- Navigation uses Unreal's default free-fly pawn. It has no collision with the
  terrain.
