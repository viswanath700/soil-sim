# SoilSim model notes

## Rankine cutting baseline

The cutting model treats the bucket as a vertical blade with width `w` (m)
penetrating to depth `d` (m). Soil unit weight `gamma` is in N/m^3, cohesion `c`
and surcharge `q` are in Pa, and the resulting resistance `F` is in N:

```
Kp = tan^2(pi/4 + phi/2)
F = w * (0.5 * gamma * d^2 * Kp
         + 2 * c * d * sqrt(Kp)
         + q * d * Kp)
```

Here `phi` is the soil friction angle in radians. The model exposes rake angle
in radians, using the caller's tool convention, but the Rankine baseline
does not use rake angle, soil-tool adhesion, or soil-tool friction. The returned
force is horizontal and the vertical component is zero for this vertical-blade
baseline. The interface is retained for a future Reece model.

The resistance is monotonic in depth, cohesion, and friction angle for the
validated nonnegative input domain. The solver uses bisection over
`[0, maxDepth]`. It returns the equality depth when the applied force is
bracketed. For nonpositive applied force it returns zero. If the applied force
exceeds the resistance at `maxDepth`, it returns `maxDepth`; this is a capped
depth result, not an equality solution.

## Heightfield and excavation baseline

The terrain is a regular row-major grid of cell heights in metres. Each height is
the terrain elevation at the cell centre. The grid stores `cellSize` in metres and
an `(originX, originY)` in metres. Bilinear samples clamp to the edge cells.
Gradients use centred finite differences with the same edge clamping. Moisture,
frozen fraction, and compaction arrays are allocated as reserved per-cell state
but are reserved for the future environment model.

The bucket footprint is a square with side length `width`, rotated by
`yaw` in radians. `BucketPose::z` is the commanded bottom elevation in metres;
soil is removed only between that elevation and the current cell height. The
constant-force solver is applied independently to each covered cell and the
result is clamped by the commanded elevation and available bucket capacity.
Bucket capacity and fill are loose volume in m^3. Removed cell volume is
in-situ volume and is multiplied by `bulkingFactor` before being added to the
bucket.

Dumping converts the carried loose volume back to in-situ-equivalent heightfield
volume by dividing by `bulkingFactor`. It distributes that volume with a
normalized cone-shaped radial profile whose limiting slope is the soil angle of
repose. Normalizing the discrete cell weights makes the material accounting
exact up to floating-point accumulation error. The accounting invariant
for uniform soil is:

```
terrain volume + (bucket loose volume + external spoil loose volume) / bulkingFactor
```

The conservation test uses no external spoil and requires relative error
at most `1e-6`. The fixed flat-terrain convergence scenario compares 0.20 m and
0.10 m cells and requires relative in-situ dig-volume difference at most 5%;
this is a regression threshold for that scenario, not a universal accuracy claim.

## CLI output formats

`steps.csv` contains `time_s`, `depth_m`, `force_N`,
`volume_removed_in_situ_m3`, and `bucket_fill_m3_loose`.
`heightfield_final.csv` contains one row per sample with `x_m`, `y_m`, and
`height_m`.

`heightfield_final.bin` is a little-endian binary file. Its header is the
8-byte ASCII magic `SOILHF01`, a fixed-width `uint32` version (`1`), fixed-width
`uint64` column and row counts, and IEEE-754 `double` values for cell size,
origin X, and origin Y. It is followed by row-major IEEE-754 `double` height
samples. The datum is the height value zero; the current format stores heights
only.
Readers reject unknown magic, unsupported versions, zero/overflowing
dimensions, malformed, and truncated files.
