#include "soilsim/soil.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace
{
using json = nlohmann::json;
constexpr double kPi = 3.14159265358979323846;

double degreesToRadians(double degrees)
{
    return degrees * kPi / 180.0;
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

void requireNonnegative(double value, const std::string& field, const std::string& context)
{
    if (value < 0.0) {
        throw std::runtime_error(context + " field '" + field + "' must be nonnegative");
    }
}

void requireAngleDegrees(double value, const std::string& field, const std::string& context)
{
    if (value < 0.0 || value > 90.0) {
        throw std::runtime_error(context + " field '" + field + "' must be between 0 and 90 degrees");
    }
}
} // namespace

namespace soilsim
{
std::vector<SoilParams> loadSoils(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open soil data file '" + path.string() + "'");
    }

    json document;
    try {
        input >> document;
    } catch (const json::parse_error& error) {
        throw std::runtime_error("Unable to parse soil data file '" + path.string() + "': " +
                                 std::string(error.what()));
    }

    const auto presets = document.find("soils");
    if (presets == document.end() || !presets->is_array()) {
        throw std::runtime_error("Soil data file must contain an array named 'soils'");
    }

    std::vector<SoilParams> result;
    result.reserve(presets->size());
    for (std::size_t index = 0; index < presets->size(); ++index) {
        const json& preset = (*presets)[index];
        const std::string context = "Soil preset at index " + std::to_string(index);
        if (!preset.is_object()) {
            throw std::runtime_error(context + " must be an object");
        }

        SoilParams soil{
            requiredString(preset, "name", context),
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            requiredString(preset, "source", context),
            requiredBoolean(preset, "verified", context)};

        if (soil.name.empty()) {
            throw std::runtime_error(context + " field 'name' must not be empty");
        }
        soil.cohesion = requiredNumber(preset, "cohesion_kPa", context) * 1000.0;
        soil.frictionAngle =
            degreesToRadians(requiredNumber(preset, "friction_angle_deg", context));
        soil.unitWeight = requiredNumber(preset, "unit_weight_N_m3", context);
        soil.adhesion = requiredNumber(preset, "adhesion_kPa", context) * 1000.0;
        const double soilToolFrictionDegrees =
            requiredNumber(preset, "soil_tool_friction_deg", context);
        soil.soilToolFriction = degreesToRadians(soilToolFrictionDegrees);
        soil.bekkerKc = requiredNumber(preset, "bekker_kc", context);
        soil.bekkerKphi = requiredNumber(preset, "bekker_kphi", context);
        soil.bekkerN = requiredNumber(preset, "bekker_n", context);
        const double angleOfReposeDegrees =
            requiredNumber(preset, "angle_of_repose_deg", context);
        soil.angleOfRepose = degreesToRadians(angleOfReposeDegrees);
        soil.bulkingFactor = requiredNumber(preset, "bulking_factor", context);
        soil.porosity = requiredNumber(preset, "porosity_m3_m3", context);
        soil.saturatedHydraulicConductivity =
            requiredNumber(preset, "saturated_hydraulic_conductivity_m_s", context);
        soil.fieldCapacity = requiredNumber(preset, "field_capacity_m3_m3", context);
        soil.optimumMoistureForCohesion =
            requiredNumber(preset, "optimum_moisture_for_cohesion_m3_m3", context);
        soil.frozenStrengthMultiplier =
            requiredNumber(preset, "frozen_strength_multiplier", context);
        soil.thermalLagTimeConstant =
            requiredNumber(preset, "thermal_lag_time_constant_s", context);

        requireNonnegative(soil.cohesion, "cohesion_kPa", context);
        requireAngleDegrees(requiredNumber(preset, "friction_angle_deg", context),
                             "friction_angle_deg", context);
        if (soil.unitWeight <= 0.0) {
            throw std::runtime_error(context + " field 'unit_weight_N_m3' must be positive");
        }
        requireNonnegative(soil.adhesion, "adhesion_kPa", context);
        requireAngleDegrees(soilToolFrictionDegrees, "soil_tool_friction_deg", context);
        requireNonnegative(soil.bekkerKc, "bekker_kc", context);
        requireNonnegative(soil.bekkerKphi, "bekker_kphi", context);
        if (soil.bekkerN <= 0.0) {
            throw std::runtime_error(context + " field 'bekker_n' must be positive");
        }
        requireAngleDegrees(angleOfReposeDegrees, "angle_of_repose_deg", context);
        if (soil.bulkingFactor <= 0.0) {
            throw std::runtime_error(context + " field 'bulking_factor' must be positive");
        }
        if (soil.porosity <= 0.0 || soil.porosity >= 1.0) {
            throw std::runtime_error(context + " field 'porosity_m3_m3' must be between 0 and 1");
        }
        requireNonnegative(soil.saturatedHydraulicConductivity,
                           "saturated_hydraulic_conductivity_m_s", context);
        if (soil.fieldCapacity <= 0.0 || soil.fieldCapacity >= soil.porosity) {
            throw std::runtime_error(context +
                                     " field 'field_capacity_m3_m3' must be between 0 and porosity");
        }
        if (soil.optimumMoistureForCohesion < 0.0 ||
            soil.optimumMoistureForCohesion > soil.porosity) {
            throw std::runtime_error(context +
                                     " field 'optimum_moisture_for_cohesion_m3_m3' must be within "
                                     "the porosity range");
        }
        if (soil.frozenStrengthMultiplier < 1.0) {
            throw std::runtime_error(context +
                                     " field 'frozen_strength_multiplier' must be at least 1");
        }
        if (soil.thermalLagTimeConstant <= 0.0) {
            throw std::runtime_error(context +
                                     " field 'thermal_lag_time_constant_s' must be positive");
        }

        result.push_back(std::move(soil));
    }
    if (result.empty()) {
        throw std::runtime_error("Soil data file must contain at least one soil preset");
    }
    return result;
}
} // namespace soilsim
