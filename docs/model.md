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

## Weather driver

`SimpleWeatherDriver` is a deterministic function of a start timestamp, an
elapsed simulation time in seconds, and one immutable weather profile. The
calendar is the proleptic Gregorian calendar for years 1 through 9999. Start
timestamps are UTC with no daylight-saving adjustments; timestamps contain a
date and seconds since UTC midnight in `[0, 86400)`. Leap years follow the
Gregorian 4/100/400 rule. Time advances across dates using elapsed seconds,
without consulting the system clock.

Profile air temperature is an illustrative smooth approximation, in degrees C:

```
Tair = Tmean
     + Aseason * cos(2*pi*(dayOfYear - seasonalPeakDay)/daysInYear)
     + Adaily * cos(2*pi*(solarHour - 15)/24)
```

`Tmean`, `Aseason`, and `Adaily` are in degrees C. The seasonal peak day is
one-based; its phase uses 365 or 366 days according to the Gregorian year. The
daily cycle has its minimum at 03:00 and maximum at 15:00 apparent solar time.
Apparent solar time is UTC clock time plus longitude/15 hours, wrapped into
`[0,24)`; the equation of time is omitted. Profile parameters are synthetic
unless a weather file provides a verified source.

Solar declination and daylight use a first-order solar-geometry approximation
based on Allen et al., FAO Irrigation and Drainage Paper 56 (1998), equations
24, 25, and 34:

```
delta = 0.409 * sin(2*pi*dayOfYear/365 - 1.39)  // radians
cosSunsetHourAngle = -tan(latitude) * tan(delta)
dayLengthHours = 24 * acos(cosSunsetHourAngle) / pi
sinElevation = sin(latitude)*sin(delta)
             + cos(latitude)*cos(delta)*cos(hourAngle)
solarFactor = clamp(sinElevation, 0, 1)
```

Latitude, declination, and hour angle are in radians. Hour angle is
`(solarHour - 12)*pi/12`. Solar factor is zero outside daylight. Values of
`cosSunsetHourAngle <= -1` represent polar day (24 hours); values `>= 1`
represent polar night (zero hours). The declination expression uses a fixed
365-day solar approximation even in a Gregorian leap year. This clear-sky
factor is a unitless illumination proxy, not irradiance or a measured weather
quantity. All solar calculations use the same apparent-solar-time convention
as the daily temperature cycle.

Scripted precipitation events have a nonnegative start and positive duration,
with rates in mm of water equivalent per hour. An event contributes on the
half-open interval `[startSeconds, endSeconds)`; overlapping rates add, and no
active event means zero precipitation. At air temperature strictly below the
configured threshold, all event precipitation is snowfall; at the threshold
and above, it is rain. Rates are not converted to water depth by the weather
driver. Weather profiles validate finite values, temperature bounds of
`[-100, 100]` degrees C, latitude and longitude bounds of `[-90, 90]` and
`[-180, 180]` degrees, and event rates from zero through 100000 mm/h.

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
