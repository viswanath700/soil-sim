#include "soilsim/moisture.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace
{
using json = nlohmann::json;
constexpr double kSecondsPerHour = 3600.0;
constexpr double kMillimetresPerMetre = 1000.0;
constexpr double kMaximumPrecipitationRateMmPerHour = 100000.0;

void requireFinite(double value, const std::string& name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument(name + " must be finite");
    }
}

double requiredNumber(const json& object, const char* key, const std::string& context)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_number()) {
        throw std::runtime_error(context + " must contain numeric field '" + key + "'");
    }
    const double value = iterator->get<double>();
    if (!std::isfinite(value)) {
        throw std::runtime_error(context + " field '" + key + "' must be finite");
    }
    return value;
}

std::string requiredString(const json& object, const char* key, const std::string& context)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_string()) {
        throw std::runtime_error(context + " must contain string field '" + key + "'");
    }
    return iterator->get<std::string>();
}

bool requiredBoolean(const json& object, const char* key, const std::string& context)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_boolean()) {
        throw std::runtime_error(context + " must contain boolean field '" + key + "'");
    }
    return iterator->get<bool>();
}

void requireOnlyMoistureKeys(const json& moisture)
{
    for (auto iterator = moisture.begin(); iterator != moisture.end(); ++iterator) {
        if (iterator.key() != "active_layer_depth_m" &&
            iterator.key() != "evaporation_rate_mm_per_h_per_c_per_solar" &&
            iterator.key() != "source" && iterator.key() != "verified") {
            throw std::runtime_error("Scenario moisture contains unknown field '" +
                                     iterator.key() + "'");
        }
    }
}

void validateParameters(const soilsim::MoistureParameters& parameters)
{
    requireFinite(parameters.activeLayerDepthM, "activeLayerDepthM");
    requireFinite(parameters.evaporationRateMmPerHourPerCPerSolar,
                  "evaporationRateMmPerHourPerCPerSolar");
    if (parameters.activeLayerDepthM <= 0.0) {
        throw std::invalid_argument("activeLayerDepthM must be positive");
    }
    if (parameters.evaporationRateMmPerHourPerCPerSolar < 0.0) {
        throw std::invalid_argument("evaporation rate coefficient must be nonnegative");
    }
    if (parameters.source.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("moisture parameter source must not be empty");
    }
}

void validateSoil(const soilsim::SoilParams& soil)
{
    requireFinite(soil.porosity, "soil.porosity");
    requireFinite(soil.saturatedHydraulicConductivity,
                  "soil.saturatedHydraulicConductivity");
    requireFinite(soil.fieldCapacity, "soil.fieldCapacity");
    if (soil.porosity <= 0.0 || soil.porosity >= 1.0) {
        throw std::invalid_argument("soil.porosity must be between 0 and 1");
    }
    if (soil.saturatedHydraulicConductivity < 0.0) {
        throw std::invalid_argument("soil.saturatedHydraulicConductivity must be nonnegative");
    }
    if (soil.fieldCapacity <= 0.0 || soil.fieldCapacity >= soil.porosity) {
        throw std::invalid_argument(
            "soil.fieldCapacity must be greater than 0 and less than porosity");
    }
}

void validateCell(const soilsim::CellState& cell, const soilsim::SoilParams& soil)
{
    requireFinite(cell.moisture, "cell.moisture");
    requireFinite(cell.temperatureC, "cell.temperatureC");
    requireFinite(cell.frozenFraction, "cell.frozenFraction");
    requireFinite(cell.snowWaterEquivalent, "cell.snowWaterEquivalent");
    if (cell.moisture < 0.0 || cell.moisture > soil.porosity) {
        throw std::invalid_argument("cell.moisture must be within [0, soil.porosity]");
    }
    if (cell.frozenFraction < 0.0 || cell.frozenFraction > 1.0) {
        throw std::invalid_argument("cell.frozenFraction must be within [0, 1]");
    }
    if (cell.snowWaterEquivalent < 0.0) {
        throw std::invalid_argument("cell.snowWaterEquivalent must be nonnegative");
    }
}

void validateInputs(const soilsim::MoistureInputs& inputs, double timeStepSeconds,
                    double snowWaterEquivalent)
{
    requireFinite(inputs.rainRateMmPerHour, "rainRateMmPerHour");
    requireFinite(inputs.snowmeltWaterM, "snowmeltWaterM");
    requireFinite(inputs.airTemperatureC, "airTemperatureC");
    requireFinite(inputs.solarFactor, "solarFactor");
    requireFinite(timeStepSeconds, "timeStepSeconds");
    if (inputs.rainRateMmPerHour < 0.0 ||
        inputs.rainRateMmPerHour > kMaximumPrecipitationRateMmPerHour) {
        throw std::invalid_argument("rainRateMmPerHour is out of range");
    }
    if (inputs.snowmeltWaterM < 0.0 || inputs.snowmeltWaterM > snowWaterEquivalent) {
        throw std::invalid_argument("snowmeltWaterM must be within available snow water equivalent");
    }
    if (inputs.airTemperatureC < -100.0 || inputs.airTemperatureC > 100.0) {
        throw std::invalid_argument("airTemperatureC must be between -100 and 100");
    }
    if (inputs.solarFactor < 0.0 || inputs.solarFactor > 1.0) {
        throw std::invalid_argument("solarFactor must be within [0, 1]");
    }
    if (timeStepSeconds <= 0.0) {
        throw std::invalid_argument("timeStepSeconds must be positive");
    }
}

double requireFiniteFlux(double flux, const char* name)
{
    if (!std::isfinite(flux) || flux < 0.0) {
        throw std::overflow_error(std::string("moisture calculation produced invalid ") + name);
    }
    return flux;
}
} // namespace

namespace soilsim
{
MoistureParameters loadMoistureParameters(const std::filesystem::path& scenarioPath)
{
    std::ifstream input(scenarioPath);
    if (!input) {
        throw std::runtime_error("Unable to open scenario file '" + scenarioPath.string() + "'");
    }

    json document;
    try {
        input >> document;
    } catch (const json::parse_error& error) {
        throw std::runtime_error("Unable to parse scenario file '" + scenarioPath.string() +
                                 "': " + std::string(error.what()));
    }
    if (!document.is_object()) {
        throw std::runtime_error("Scenario must be a JSON object");
    }
    const auto moisture = document.find("moisture");
    if (moisture == document.end() || !moisture->is_object()) {
        throw std::runtime_error("Scenario must contain a 'moisture' object");
    }
    requireOnlyMoistureKeys(*moisture);

    MoistureParameters parameters{
        requiredNumber(*moisture, "active_layer_depth_m", "Scenario moisture"),
        requiredNumber(*moisture, "evaporation_rate_mm_per_h_per_c_per_solar",
                       "Scenario moisture"),
        requiredString(*moisture, "source", "Scenario moisture"),
        requiredBoolean(*moisture, "verified", "Scenario moisture")};
    try {
        validateParameters(parameters);
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error("Invalid moisture parameters in scenario '" +
                                 scenarioPath.string() + "': " + std::string(error.what()));
    }
    return parameters;
}

MoistureFluxes MoistureModel::update(CellState& cell, const SoilParams& soil,
                                     const MoistureParameters& parameters,
                                     const MoistureInputs& inputs,
                                     double timeStepSeconds) const
{
    validateSoil(soil);
    validateParameters(parameters);
    validateCell(cell, soil);
    validateInputs(inputs, timeStepSeconds, cell.snowWaterEquivalent);
    double updatedMoisture = cell.moisture;

    const double rainInputM = requireFiniteFlux(
        inputs.rainRateMmPerHour * timeStepSeconds /
            (kMillimetresPerMetre * kSecondsPerHour),
        "rain input");
    const double totalInputM = requireFiniteFlux(rainInputM + inputs.snowmeltWaterM,
                                                "combined water input");
    const double conductivityCapacityM =
        requireFiniteFlux(soil.saturatedHydraulicConductivity * timeStepSeconds,
                          "infiltration capacity");
    const double poreCapacityM =
        requireFiniteFlux((soil.porosity - updatedMoisture) * parameters.activeLayerDepthM,
                          "available pore capacity");
    const double infiltrationM =
        std::min({totalInputM, conductivityCapacityM, poreCapacityM});
    const double runoffM = totalInputM - infiltrationM;

    double storedWaterM = updatedMoisture * parameters.activeLayerDepthM + infiltrationM;
    if (poreCapacityM > 0.0 && infiltrationM == poreCapacityM) {
        updatedMoisture = soil.porosity;
        storedWaterM = soil.porosity * parameters.activeLayerDepthM;
    } else if (infiltrationM > 0.0) {
        updatedMoisture = storedWaterM / parameters.activeLayerDepthM;
    }

    const double potentialEvaporationM = requireFiniteFlux(
        parameters.evaporationRateMmPerHourPerCPerSolar *
            std::max(inputs.airTemperatureC, 0.0) * inputs.solarFactor * timeStepSeconds /
            (kMillimetresPerMetre * kSecondsPerHour),
        "potential evaporation");
    const double waterBeforeEvaporationM = storedWaterM;
    const double evaporationM = std::min(storedWaterM, potentialEvaporationM);
    storedWaterM -= evaporationM;
    if (evaporationM > 0.0 && evaporationM == waterBeforeEvaporationM) {
        updatedMoisture = 0.0;
    } else if (evaporationM > 0.0) {
        updatedMoisture = storedWaterM / parameters.activeLayerDepthM;
    }

    const double waterAboveFieldCapacityM =
        std::max(updatedMoisture - soil.fieldCapacity, 0.0) * parameters.activeLayerDepthM;
    const double drainageFraction =
        (updatedMoisture - soil.fieldCapacity) / (soil.porosity - soil.fieldCapacity);
    const double potentialDrainageM = requireFiniteFlux(
        soil.saturatedHydraulicConductivity * timeStepSeconds *
            std::clamp(drainageFraction, 0.0, 1.0),
        "potential drainage");
    const double drainageM = std::min(waterAboveFieldCapacityM, potentialDrainageM);
    if (waterAboveFieldCapacityM > 0.0 && drainageM == waterAboveFieldCapacityM) {
        updatedMoisture = soil.fieldCapacity;
    } else if (drainageM > 0.0) {
        storedWaterM -= drainageM;
        updatedMoisture = storedWaterM / parameters.activeLayerDepthM;
    }

    const double updatedSnowWaterEquivalent =
        inputs.snowmeltWaterM == cell.snowWaterEquivalent
            ? 0.0
            : cell.snowWaterEquivalent - inputs.snowmeltWaterM;
    if (!std::isfinite(updatedMoisture) || updatedMoisture < 0.0 ||
        updatedMoisture > soil.porosity || !std::isfinite(updatedSnowWaterEquivalent)) {
        throw std::runtime_error("moisture update produced state outside physical bounds");
    }

    const MoistureFluxes fluxes{rainInputM,
                                inputs.snowmeltWaterM,
                                infiltrationM,
                                requireFiniteFlux(runoffM, "runoff"),
                                requireFiniteFlux(evaporationM, "evaporation"),
                                requireFiniteFlux(drainageM, "drainage")};
    cell.moisture = updatedMoisture;
    cell.snowWaterEquivalent = updatedSnowWaterEquivalent;
    return fluxes;
}
} // namespace soilsim
