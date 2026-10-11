#include "soilsim/excavate.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <algorithm>

namespace
{
const std::filesystem::path kSoilData =
    std::filesystem::path(SOILSIM_SOURCE_DIR) / "data" / "soils.json";

soilsim::SoilParams loadDrySand()
{
    const auto soils = soilsim::loadSoils(kSoilData);
    return soils.front();
}

void runScenario(soilsim::Heightfield& terrain, soilsim::BucketState& bucket,
                 const soilsim::SoilParams& soil)
{
    const soilsim::RankineCuttingModel model;
    const soilsim::BucketPose pose{1.25, 1.25, 0.5, 0.0, 0.0, 0.5};
    soilsim::digStep(terrain, bucket, soil, model, pose, 1e6, 0.1);
    soilsim::dump(terrain, bucket, soil, 3.75, 3.75);
}
} // namespace

TEST_CASE("dig and dump conserve in-situ-equivalent material", "[excavate]")
{
    const auto soil = loadDrySand();
    soilsim::Heightfield terrain(10, 10, 0.5, 0.0, 0.0, 1.0);
    soilsim::BucketState bucket{10.0, 0.0};
    const double initialVolume = terrain.volumeAboveDatum();

    const soilsim::RankineCuttingModel model;
    const soilsim::BucketPose pose{1.25, 1.25, 0.5, 0.0, 0.0, 0.5};
    const auto result = soilsim::digStep(terrain, bucket, soil, model, pose, 1e6, 0.1);
    REQUIRE(result.volumeRemoved > 0.0);
    const double carriedBeforeDump = bucket.carriedVolume;
    soilsim::dump(terrain, bucket, soil, 3.75, 3.75);

    const double relativeError =
        std::abs(terrain.volumeAboveDatum() - initialVolume) / initialVolume;
    REQUIRE(relativeError <= 1e-6);
    REQUIRE(bucket.carriedVolume == Catch::Approx(0.0));
    REQUIRE(carriedBeforeDump == Catch::Approx(result.volumeRemoved * soil.bulkingFactor));
}

TEST_CASE("dig respects loose-volume bucket capacity", "[excavate]")
{
    const auto soil = loadDrySand();
    soilsim::Heightfield terrain(10, 10, 0.5, 0.0, 0.0, 1.0);
    soilsim::BucketState bucket{0.05, 0.0};
    const soilsim::RankineCuttingModel model;
    const soilsim::BucketPose pose{1.25, 1.25, 0.0, 0.0, 0.0, 0.5};

    const auto result = soilsim::digStep(terrain, bucket, soil, model, pose, 1e6, 0.1);
    REQUIRE(bucket.carriedVolume <= bucket.capacity);
    REQUIRE(bucket.carriedVolume == Catch::Approx(bucket.capacity));
    REQUIRE(result.volumeRemoved == Catch::Approx(bucket.capacity / soil.bulkingFactor));
    REQUIRE(terrain.at(2, 2) > 0.0);
}

TEST_CASE("fixed flat scenario is stable across documented cell sizes", "[excavate]")
{
    const auto soil = loadDrySand();
    const soilsim::RankineCuttingModel model;
    auto run = [&](double cellSize, std::size_t dimension) {
        soilsim::Heightfield terrain(dimension, dimension, cellSize, 0.0, 0.0, 1.0);
        soilsim::BucketState bucket{100.0, 0.0};
        const soilsim::BucketPose pose{2.0, 2.0, 0.7, 0.0, 0.0, 0.4};
        return soilsim::digStep(terrain, bucket, soil, model, pose, 1e6, 0.1).volumeRemoved;
    };

    const double coarseVolume = run(0.2, 20);
    const double fineVolume = run(0.1, 40);
    const double relativeDifference =
        std::abs(coarseVolume - fineVolume) / std::max(coarseVolume, fineVolume);
    REQUIRE(relativeDifference <= 0.05);
}

TEST_CASE("dig and dump output is deterministic", "[excavate]")
{
    const auto soil = loadDrySand();
    soilsim::Heightfield first(10, 10, 0.5, 0.0, 0.0, 1.0);
    soilsim::Heightfield second(10, 10, 0.5, 0.0, 0.0, 1.0);
    soilsim::BucketState firstBucket{10.0, 0.0};
    soilsim::BucketState secondBucket{10.0, 0.0};

    runScenario(first, firstBucket, soil);
    runScenario(second, secondBucket, soil);

    REQUIRE(first.serializedBytes() == second.serializedBytes());
    REQUIRE(firstBucket.capacity == secondBucket.capacity);
    REQUIRE(firstBucket.carriedVolume == secondBucket.carriedVolume);
}
