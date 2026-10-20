#pragma once

namespace soilsim
{
struct CellState
{
    double moisture;
    double temperatureC;
    double frozenFraction;
    double snowWaterEquivalent;
};
} // namespace soilsim
