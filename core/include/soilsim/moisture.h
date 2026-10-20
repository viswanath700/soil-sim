#pragma once

#include "soilsim/environment.h"
#include "soilsim/soil.h"

#include <filesystem>
#include <string>

namespace soilsim
{
struct MoistureParameters
{
    double activeLayerDepthM;
    double evaporationRateMmPerHourPerCPerSolar;
    std::string source;
    bool verified;
};

struct MoistureInputs
{
    double rainRateMmPerHour;
    double snowmeltWaterM;
    double airTemperatureC;
    double solarFactor;
};

struct MoistureFluxes
{
    double rainInputM;
    double snowmeltInputM;
    double infiltrationM;
    double runoffM;
    double evaporationM;
    double drainageM;
};

MoistureParameters loadMoistureParameters(const std::filesystem::path& scenarioPath);

class MoistureModel
{
public:
    MoistureFluxes update(CellState& cell, const SoilParams& soil,
                          const MoistureParameters& parameters, const MoistureInputs& inputs,
                          double timeStepSeconds) const;
};
} // namespace soilsim
