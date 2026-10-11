#include "soilsim/heightfield.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("heightfield samples, differentiates, and measures volume", "[heightfield]")
{
    soilsim::Heightfield terrain(3, 3, 1.0);
    for (std::size_t row = 0; row < terrain.rows(); ++row) {
        for (std::size_t column = 0; column < terrain.columns(); ++column) {
            const double x = static_cast<double>(column) + 0.5;
            const double y = static_cast<double>(row) + 0.5;
            terrain.at(column, row) = x + 2.0 * y;
        }
    }

    REQUIRE(terrain.sampleBilinear(1.0, 1.0) == Catch::Approx(3.0));
    const auto slope = terrain.gradient(1.5, 1.5);
    REQUIRE(slope.first == Catch::Approx(1.0));
    REQUIRE(slope.second == Catch::Approx(2.0));
    REQUIRE(terrain.volumeAboveDatum(0.0) == Catch::Approx(40.5));
}

TEST_CASE("heightfield serialized bytes are stable", "[heightfield]")
{
    soilsim::Heightfield terrain(2, 2, 0.25, 1.0, 2.0, 3.0);
    REQUIRE(terrain.serializedBytes() == terrain.serializedBytes());
    REQUIRE(terrain.heights().size() == 4);
}
