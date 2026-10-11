#include "soilsim/cutting.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>

namespace
{
const std::filesystem::path kSoilData =
    std::filesystem::path(SOILSIM_SOURCE_DIR) / "data" / "soils.json";
constexpr double kPi = 3.14159265358979323846;

soilsim::SoilParams testSoil(double cohesion, double frictionAngle)
{
    soilsim::SoilParams soil{};
    soil.name = "test";
    soil.cohesion = cohesion;
    soil.frictionAngle = frictionAngle;
    soil.unitWeight = 16000.0;
    soil.adhesion = 0.0;
    soil.soilToolFriction = 0.0;
    soil.bekkerN = 1.0;
    soil.bulkingFactor = 1.2;
    return soil;
}
} // namespace

TEST_CASE("Rankine force matches the phi-zero hand calculation", "[cutting]")
{
    const soilsim::SoilParams soil = testSoil(10000.0, 0.0);
    const soilsim::RankineCuttingModel model;
    const double depth = 0.5;
    const double width = 2.0;
    const double surcharge = 5000.0;
    const double expected =
        width * (0.5 * soil.unitWeight * depth * depth + 2.0 * soil.cohesion * depth +
                 surcharge * depth);

    const auto result = model.cut(soil, depth, width, 0.0, surcharge);
    REQUIRE(result.force == Catch::Approx(expected));
    REQUIRE(result.forceHorizontal == Catch::Approx(expected));
    REQUIRE(result.forceVertical == Catch::Approx(0.0));
}

TEST_CASE("zero-cohesion sand force is proportional to depth squared", "[cutting]")
{
    const soilsim::SoilParams soil = testSoil(0.0, 30.0 * kPi / 180.0);
    const soilsim::RankineCuttingModel model;

    const double forceAtOne = model.cut(soil, 0.4, 1.0, 0.0, 0.0).force;
    const double forceAtTwo = model.cut(soil, 0.8, 1.0, 0.0, 0.0).force;
    REQUIRE(forceAtTwo == Catch::Approx(4.0 * forceAtOne));
}

TEST_CASE("Rankine force increases with depth, friction, and cohesion", "[cutting]")
{
    const soilsim::RankineCuttingModel model;
    const auto low = model.cut(testSoil(5000.0, 20.0 * kPi / 180.0), 0.5, 1.0, 0.0, 0.0);
    const auto deep = model.cut(testSoil(5000.0, 20.0 * kPi / 180.0), 0.7, 1.0, 0.0, 0.0);
    const auto moreFriction =
        model.cut(testSoil(5000.0, 30.0 * kPi / 180.0), 0.5, 1.0, 0.0, 0.0);
    const auto moreCohesion =
        model.cut(testSoil(10000.0, 20.0 * kPi / 180.0), 0.5, 1.0, 0.0, 0.0);

    REQUIRE(deep.force > low.force);
    REQUIRE(moreFriction.force > low.force);
    REQUIRE(moreCohesion.force > low.force);
}

TEST_CASE("constant-force solver converges and caps at max depth", "[cutting]")
{
    const auto soils = soilsim::loadSoils(kSoilData);
    const soilsim::RankineCuttingModel model;
    for (const auto& soil : soils) {
        const double maxDepth = 1.0;
        const double targetDepth = 0.35;
        const double targetForce = model.cut(soil, targetDepth, 1.0, 0.0, 0.0).force;
        const double solvedDepth =
            soilsim::solveDepthForForce(model, soil, targetForce, 1.0, 0.0, 0.0, maxDepth);
        REQUIRE(solvedDepth == Catch::Approx(targetDepth).margin(2e-6));
    }

    REQUIRE(soilsim::solveDepthForForce(model, soils[0], 0.0, 1.0, 0.0, 0.0, 1.0) ==
            Catch::Approx(0.0));
    REQUIRE(soilsim::solveDepthForForce(model, soils[0], -1.0, 1.0, 0.0, 0.0, 1.0) ==
            Catch::Approx(0.0));
    REQUIRE(soilsim::solveDepthForForce(model, soils[0], 1e12, 1.0, 0.0, 0.0, 1.0) ==
            Catch::Approx(1.0));
}

TEST_CASE("cutting model rejects invalid inputs", "[cutting]")
{
    const soilsim::RankineCuttingModel model;
    const auto soil = testSoil(0.0, 0.0);

    REQUIRE_THROWS(model.cut(soil, -1.0, 1.0, 0.0, 0.0));
    REQUIRE_THROWS(model.cut(soil, 1.0, 0.0, 0.0, 0.0));
    REQUIRE_THROWS(model.cut(soil, 1.0, 1.0, 0.0, -1.0));
    REQUIRE_THROWS(soilsim::solveDepthForForce(model, soil, 1.0, 1.0, 0.0, 0.0, 0.0));
    REQUIRE_THROWS(soilsim::solveDepthForForce(model, soil, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0));
}
