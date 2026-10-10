#include "soilsim/types.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("project metadata is valid", "[smoke]") {
    REQUIRE(soilsim::projectName() == "soilsim");
    REQUIRE(soilsim::projectVersion() == "0.1.0");
}
