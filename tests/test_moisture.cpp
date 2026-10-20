#include "soilsim/moisture.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace
{
const std::filesystem::path kSoilData =
    std::filesystem::path(SOILSIM_SOURCE_DIR) / "data" / "soils.json";
const std::filesystem::path kSampleScenario =
    std::filesystem::path(SOILSIM_SOURCE_DIR) / "data" / "scenarios" / "sample.json";

soilsim::MoistureParameters parameters(double activeLayerDepthM = 0.3,
                                       double evaporationRate = 0.01)
{
    return {activeLayerDepthM, evaporationRate, "illustrative test parameters", false};
}

soilsim::SoilParams drySand()
{
    return soilsim::loadSoils(kSoilData).front();
}

std::filesystem::path writeTemporaryJson(const std::string& contents)
{
    const auto path = std::filesystem::temp_directory_path() /
                      "soilsim_invalid_moisture_scenario.json";
    std::ofstream output(path);
    output << contents;
    return path;
}
} // namespace

TEST_CASE("moisture parameters load from the scenario with provenance", "[moisture]")
{
    const auto loaded = soilsim::loadMoistureParameters(kSampleScenario);
    REQUIRE(loaded.activeLayerDepthM == Catch::Approx(0.3));
    REQUIRE(loaded.evaporationRateMmPerHourPerCPerSolar == Catch::Approx(0.01));
    REQUIRE_FALSE(loaded.verified);
    REQUIRE_FALSE(loaded.source.empty());
}

TEST_CASE("rain infiltration is limited by conductivity and available pore space", "[moisture]")
{
    soilsim::SoilParams soil = drySand();
    soil.saturatedHydraulicConductivity = 1.0e-6;
    soilsim::CellState cell{0.1, 10.0, 0.0, 0.0};
    const soilsim::MoistureModel model;

    const auto fluxes = model.update(cell, soil, parameters(),
                                     soilsim::MoistureInputs{30.0, 0.0, 10.0, 0.0}, 3600.0);

    REQUIRE(fluxes.rainInputM == Catch::Approx(0.03));
    REQUIRE(fluxes.infiltrationM == Catch::Approx(0.0036));
    REQUIRE(fluxes.runoffM > 0.0);
    REQUIRE(fluxes.infiltrationM + fluxes.runoffM == Catch::Approx(fluxes.rainInputM));
    REQUIRE(cell.moisture >= 0.0);
    REQUIRE(cell.moisture <= soil.porosity);
}

TEST_CASE("rain response is monotonic and dry soil evaporates without going below zero",
          "[moisture]")
{
    const soilsim::SoilParams soil = drySand();
    const soilsim::MoistureModel model;
    const soilsim::CellState initial{0.05, 10.0, 0.0, 0.0};
    auto lowRainCell = initial;
    auto highRainCell = initial;

    model.update(lowRainCell, soil, parameters(),
                 soilsim::MoistureInputs{1.0, 0.0, 10.0, 0.0}, 3600.0);
    model.update(highRainCell, soil, parameters(),
                 soilsim::MoistureInputs{10.0, 0.0, 10.0, 0.0}, 3600.0);
    REQUIRE(highRainCell.moisture > lowRainCell.moisture);

    auto dryingCell = initial;
    const auto dryingFluxes =
        model.update(dryingCell, soil, parameters(),
                    soilsim::MoistureInputs{0.0, 0.0, 30.0, 1.0}, 3600.0);
    REQUIRE(dryingFluxes.evaporationM > 0.0);
    REQUIRE(dryingFluxes.drainageM == Catch::Approx(0.0));
    REQUIRE(dryingCell.moisture < initial.moisture);
    REQUIRE(dryingCell.moisture >= 0.0);
}

TEST_CASE("closed water boundaries preserve a cell below field capacity", "[moisture]")
{
    const soilsim::SoilParams soil = drySand();
    soilsim::CellState cell{0.05, 10.0, 0.0, 0.0};
    const soilsim::CellState initial = cell;
    const soilsim::MoistureModel model;

    const auto fluxes = model.update(cell, soil, parameters(),
                                     soilsim::MoistureInputs{0.0, 0.0, 10.0, 0.0}, 3600.0);

    REQUIRE(cell.moisture == Catch::Approx(initial.moisture));
    REQUIRE(fluxes.runoffM == Catch::Approx(0.0));
    REQUIRE(fluxes.evaporationM == Catch::Approx(0.0));
    REQUIRE(fluxes.drainageM == Catch::Approx(0.0));
}

TEST_CASE("saturated cells route incoming water to runoff without exceeding porosity",
          "[moisture]")
{
    soilsim::SoilParams soil = drySand();
    soil.saturatedHydraulicConductivity = 0.0;
    soilsim::CellState cell{soil.porosity, 10.0, 0.0, 0.0};
    const soilsim::MoistureModel model;

    const auto fluxes = model.update(cell, soil, parameters(),
                                     soilsim::MoistureInputs{5.0, 0.0, 10.0, 0.0}, 3600.0);

    REQUIRE(fluxes.infiltrationM == Catch::Approx(0.0));
    REQUIRE(fluxes.runoffM == Catch::Approx(0.005));
    REQUIRE(cell.moisture == Catch::Approx(soil.porosity));
}

TEST_CASE("rain, snowfall, snowmelt, evaporation, drainage, and runoff balance per step",
          "[moisture]")
{
    const soilsim::SoilParams soil = drySand();
    const soilsim::MoistureParameters moistureParameters = parameters(0.5, 0.01);
    soilsim::CellState cell{0.15, 8.0, 0.0, 0.005};
    constexpr double cellAreaM2 = 2.25;
    const double initialSoilWaterM = cell.moisture * moistureParameters.activeLayerDepthM;
    const double initialSnowWaterEquivalentM = cell.snowWaterEquivalent;

    const double snowfallInputM = 0.004;
    cell.snowWaterEquivalent += snowfallInputM;
    constexpr double snowmeltM = 0.002;
    const soilsim::MoistureModel model;
    const auto fluxes = model.update(
        cell, soil, moistureParameters,
        soilsim::MoistureInputs{3.0, snowmeltM, 20.0, 0.5}, 3600.0);

    const double totalInputM =
        (initialSoilWaterM + initialSnowWaterEquivalentM + snowfallInputM +
         fluxes.rainInputM) *
        cellAreaM2;
    const double accountedWaterM =
        (cell.moisture * moistureParameters.activeLayerDepthM +
         cell.snowWaterEquivalent + fluxes.runoffM + fluxes.evaporationM +
         fluxes.drainageM) *
        cellAreaM2;
    REQUIRE(std::abs(totalInputM - accountedWaterM) / totalInputM <= 1.0e-9);
    REQUIRE(cell.snowWaterEquivalent == Catch::Approx(0.007));
    REQUIRE(fluxes.snowmeltInputM == Catch::Approx(snowmeltM));
    REQUIRE(fluxes.runoffM >= 0.0);
    REQUIRE(fluxes.evaporationM >= 0.0);
    REQUIRE(fluxes.drainageM >= 0.0);
}

TEST_CASE("moisture update rejects invalid state, fluxes, and timestep", "[moisture]")
{
    const soilsim::SoilParams soil = drySand();
    const soilsim::MoistureModel model;

    soilsim::CellState invalidMoisture{soil.porosity + 0.01, 0.0, 0.0, 0.0};
    REQUIRE_THROWS(model.update(invalidMoisture, soil, parameters(),
                                soilsim::MoistureInputs{0.0, 0.0, 0.0, 0.0}, 1.0));

    soilsim::CellState insufficientSnow{0.1, 0.0, 0.0, 0.001};
    REQUIRE_THROWS(model.update(insufficientSnow, soil, parameters(),
                                soilsim::MoistureInputs{0.0, 0.002, 0.0, 0.0}, 1.0));

    soilsim::CellState validCell{0.1, 0.0, 0.0, 0.0};
    REQUIRE_THROWS(model.update(validCell, soil, parameters(),
                                soilsim::MoistureInputs{0.0, 0.0, 0.0, 1.1}, 1.0));
    REQUIRE_THROWS(model.update(validCell, soil, parameters(0.0),
                                soilsim::MoistureInputs{0.0, 0.0, 0.0, 0.0}, 1.0));
    REQUIRE_THROWS(model.update(validCell, soil, parameters(),
                                soilsim::MoistureInputs{0.0, 0.0, 0.0, 0.0}, 0.0));
    REQUIRE_THROWS(model.update(validCell, soil, parameters(),
                                soilsim::MoistureInputs{
                                    std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0},
                                1.0));
}

TEST_CASE("moisture configuration loader rejects an invalid active layer", "[moisture]")
{
    const auto path = writeTemporaryJson(R"json({
        "moisture": {
            "active_layer_depth_m": 0,
            "evaporation_rate_mm_per_h_per_c_per_solar": 0.01,
            "source": "test",
            "verified": false
        }
    })json");
    REQUIRE_THROWS_WITH(soilsim::loadMoistureParameters(path),
                        Catch::Matchers::ContainsSubstring("activeLayerDepthM"));
    std::filesystem::remove(path);
}
