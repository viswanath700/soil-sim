# SoilSim model notes

## Week 1 Rankine cutting baseline

The Week 1 cutting model treats the bucket as a vertical blade with width `w` (m)
penetrating to depth `d` (m). Soil unit weight `gamma` is in N/m^3, cohesion `c`
and surcharge `q` are in Pa, and the resulting resistance `F` is in N:

```
Kp = tan^2(pi/4 + phi/2)
F = w * (0.5 * gamma * d^2 * Kp
         + 2 * c * d * sqrt(Kp)
         + q * d * Kp)
```

Here `phi` is the soil friction angle in radians. The model exposes rake angle
in radians, using the caller's tool convention, but the Week 1 Rankine baseline
does not use rake angle, soil-tool adhesion, or soil-tool friction. The returned
force is horizontal and the vertical component is zero for this vertical-blade
baseline. The interface is retained for the Week 2 Reece model.

The resistance is monotonic in depth, cohesion, and friction angle for the
validated nonnegative input domain. The solver uses bisection over
`[0, maxDepth]`. It returns the equality depth when the applied force is
bracketed. For nonpositive applied force it returns zero. If the applied force
exceeds the resistance at `maxDepth`, it returns `maxDepth`; this is a capped
depth result, not an equality solution.

## Week 1 heightfield and excavation baseline

The terrain is a regular row-major grid of cell heights in metres. Each height is
the terrain elevation at the cell centre. The grid stores `cellSize` in metres and
an `(originX, originY)` in metres. Bilinear samples clamp to the edge cells.
Gradients use centred finite differences with the same edge clamping. Moisture,
frozen fraction, and compaction arrays are allocated as reserved per-cell state
but are unused until Week 2.

The Day 4 bucket footprint is a square with side length `width`, rotated by
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
exact up to floating-point accumulation error. The Week 1 accounting invariant
for uniform soil is:

```
terrain volume + (bucket loose volume + external spoil loose volume) / bulkingFactor
```

The Day 4 conservation test uses no external spoil and requires relative error
at most `1e-6`. The fixed flat-terrain convergence scenario compares 0.20 m and
0.10 m cells and requires relative in-situ dig-volume difference at most 5%;
this is a regression threshold for that scenario, not a universal accuracy claim.
