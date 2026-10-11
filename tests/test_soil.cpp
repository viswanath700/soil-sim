#include "soilsim/soil.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
const std::filesystem::path kSoilData =
    std::filesystem::path(SOILSIM_SOURCE_DIR) / "data" / "soils.json";

std::filesystem::path writeTemporaryJson(const std::string& contents)
{
    const auto path = std::filesystem::temp_directory_path() / "soilsim_invalid_soils.json";
    std::ofstream output(path);
    output << contents;
    return path;
}
} // namespace

TEST_CASE("all soil presets load and convert to SI units", "[soil]")
{
    const auto soils = soilsim::loadSoils(kSoilData);

    REQUIRE(soils.size() == 5);
    REQUIRE(soils[0].name == "dry_sand");
    REQUIRE(soils[0].cohesion == 0.0);
    REQUIRE(soils[0].frictionAngle == Catch::Approx(30.0 * 3.14159265358979323846 / 180.0));
    REQUIRE(soils[0].adhesion == Catch::Approx(0.0));
    REQUIRE(soils[0].unitWeight == Catch::Approx(16000.0));
    REQUIRE(soils[0].verified == false);
    REQUIRE_FALSE(soils[0].source.empty());
}

TEST_CASE("soil loader rejects invalid physical values", "[soil]")
{
    const auto path = writeTemporaryJson(R"json({
        "soils": [{
            "name": "invalid",
            "cohesion_kPa": 0,
            "friction_angle_deg": 45,
            "unit_weight_N_m3": 0,
            "adhesion_kPa": 0,
            "soil_tool_friction_deg": 0,
            "bekker_kc": 0,
            "bekker_kphi": 0,
            "bekker_n": 0,
            "angle_of_repose_deg": 30,
            "bulking_factor": 1.2,
            "source": "test",
            "verified": false
        }]
    })json");

    REQUIRE_THROWS_WITH(soilsim::loadSoils(path), Catch::Matchers::ContainsSubstring("unit_weight"));
    std::filesystem::remove(path);
}

TEST_CASE("soil loader reports missing fields", "[soil]")
{
    const auto path = writeTemporaryJson(R"json({"soils":[{"name":"incomplete"}]})json");

    REQUIRE_THROWS_WITH(soilsim::loadSoils(path), Catch::Matchers::ContainsSubstring("source"));
    std::filesystem::remove(path);
}
