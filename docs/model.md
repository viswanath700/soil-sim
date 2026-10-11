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
