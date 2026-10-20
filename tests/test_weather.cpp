#include "soilsim/weather.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
soilsim::WeatherProfile makeProfile(
    double meanAirTemperatureC = 10.0, double dailyTemperatureAmplitudeC = 0.0,
    double seasonalTemperatureAmplitudeC = 0.0, unsigned seasonalPeakDay = 1,
    double latitudeDeg = 0.0, double longitudeDeg = 0.0, double rainSnowThresholdC = 0.0,
    std::vector<soilsim::PrecipitationEvent> precipitationEvents = {})
{
    return soilsim::WeatherProfile(soilsim::WeatherProfileData{
        meanAirTemperatureC, dailyTemperatureAmplitudeC, seasonalTemperatureAmplitudeC,
        seasonalPeakDay, latitudeDeg, longitudeDeg, rainSnowThresholdC,
        std::move(precipitationEvents), "test profile", false});
}

std::filesystem::path writeTemporaryJson(const std::string& contents)
{
    const auto path = std::filesystem::temp_directory_path() / "soilsim_weather_profile_test.json";
    std::ofstream output(path);
    output << contents;
    return path;
}

const std::filesystem::path kDemoProfile =
    std::filesystem::path(SOILSIM_SOURCE_DIR) / "data" / "weather" / "demo.json";
} // namespace

TEST_CASE("weather profile loads and produces deterministic weather", "[weather]")
{
    const auto profile = soilsim::loadWeatherProfile(kDemoProfile);
    const soilsim::SimpleWeatherDriver driver({2024, 1, 1, 0.0}, profile);

    const auto first = driver.at(3600.0);
    const auto repeated = driver.at(3600.0);
    REQUIRE(first.airTemperatureC == Catch::Approx(repeated.airTemperatureC));
    REQUIRE(first.solarFactor == Catch::Approx(repeated.solarFactor));
    REQUIRE(first.rainRateMmPerHour == Catch::Approx(2.0));
    REQUIRE(first.snowfallRateMmPerHour == Catch::Approx(0.0));
    REQUIRE(first.solarFactor >= 0.0);
    REQUIRE(first.solarFactor <= 1.0);
    REQUIRE(driver.at(7200.0).rainRateMmPerHour == Catch::Approx(0.0));
}

TEST_CASE("daily temperature reaches its configured minimum and afternoon maximum", "[weather]")
{
    const soilsim::SimpleWeatherDriver driver(
        {2025, 1, 1, 0.0}, makeProfile(10.0, 5.0, 2.0, 1));

    REQUIRE(driver.at(15.0 * 3600.0).airTemperatureC == Catch::Approx(17.0));
    REQUIRE(driver.at(3.0 * 3600.0).airTemperatureC == Catch::Approx(7.0));
}

TEST_CASE("solar factor follows day length and approximate sun elevation", "[weather]")
{
    const soilsim::SimpleWeatherDriver driver(
        {2025, 3, 20, 0.0}, makeProfile(10.0, 0.0, 0.0, 1, 0.0));

    REQUIRE(driver.at(12.0 * 3600.0).solarFactor > 0.99);
    REQUIRE(driver.at(0.0).solarFactor == Catch::Approx(0.0));
    REQUIRE(driver.at(12.0 * 3600.0).solarFactor <= 1.0);
}

TEST_CASE("calendar rollover uses the proleptic Gregorian leap-year calendar", "[weather]")
{
    const soilsim::SimpleWeatherDriver driver(
        {2024, 2, 28, 0.0}, makeProfile(10.0, 0.0, 10.0, 60));

    const double leapDayTemperature = driver.at(24.0 * 3600.0).airTemperatureC;
    const double followingDayTemperature = driver.at(48.0 * 3600.0).airTemperatureC;
    REQUIRE(leapDayTemperature == Catch::Approx(20.0));
    REQUIRE(followingDayTemperature < leapDayTemperature);
}

TEST_CASE("precipitation intervals are half-open, additive, and partition at threshold", "[weather]")
{
    const soilsim::SimpleWeatherDriver driver(
        {2025, 1, 1, 0.0},
        makeProfile(0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0,
                    {{0.0, 10.0, 2.0}, {5.0, 15.0, 3.0}}));

    REQUIRE(driver.at(4.0).rainRateMmPerHour == Catch::Approx(2.0));
    REQUIRE(driver.at(5.0).rainRateMmPerHour == Catch::Approx(5.0));
    REQUIRE(driver.at(10.0).rainRateMmPerHour == Catch::Approx(3.0));
    REQUIRE(driver.at(15.0).rainRateMmPerHour == Catch::Approx(0.0));
    REQUIRE(driver.at(5.0).snowfallRateMmPerHour == Catch::Approx(0.0));

    const soilsim::SimpleWeatherDriver snowDriver(
        {2025, 1, 1, 0.0}, makeProfile(-1.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0,
                                      {{0.0, 10.0, 2.0}}));
    REQUIRE(snowDriver.at(5.0).rainRateMmPerHour == Catch::Approx(0.0));
    REQUIRE(snowDriver.at(5.0).snowfallRateMmPerHour == Catch::Approx(2.0));
}

TEST_CASE("weather driver rejects invalid profile values and time inputs", "[weather]")
{
    REQUIRE_THROWS(makeProfile(101.0));
    REQUIRE_THROWS(makeProfile(10.0, -1.0));
    REQUIRE_THROWS(makeProfile(10.0, 0.0, 0.0, 1, 91.0));
    REQUIRE_THROWS(makeProfile(
        10.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, {{0.0, 0.0, 1.0}}));
    REQUIRE_THROWS(makeProfile(
        10.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0,
        {{0.0, 1.0, std::numeric_limits<double>::infinity()}}));
    REQUIRE_THROWS(soilsim::SimpleWeatherDriver({2023, 2, 29, 0.0}, makeProfile()));
    REQUIRE_THROWS(soilsim::SimpleWeatherDriver({2024, 1, 1, 86400.0}, makeProfile()));

    const soilsim::SimpleWeatherDriver driver({2024, 1, 1, 0.0}, makeProfile());
    REQUIRE_THROWS(driver.at(-1.0));
    REQUIRE_THROWS(driver.at(std::numeric_limits<double>::infinity()));
}

TEST_CASE("weather profile loader rejects malformed and unknown fields", "[weather]")
{
    const auto malformedPath = writeTemporaryJson(R"json({
        "source": "test",
        "verified": false,
        "air_temperature": {
            "mean_c": 10,
            "daily_amplitude_c": 0,
            "seasonal_amplitude_c": 0,
            "seasonal_peak_day": 1
        },
        "solar": {"latitude_deg": 0, "longitude_deg": 0},
        "rain_snow_threshold_c": 0,
        "precipitation_events": [
            {"start_seconds": 2, "end_seconds": 1, "rate_mm_per_h": 1}
        ]
    })json");
    REQUIRE_THROWS_WITH(soilsim::loadWeatherProfile(malformedPath),
                        Catch::Matchers::ContainsSubstring("positive duration"));
    std::filesystem::remove(malformedPath);

    const auto unknownFieldPath = writeTemporaryJson(R"json({
        "source": "test",
        "verified": false,
        "air_temperature": {
            "mean_c": 10,
            "daily_amplitude_c": 0,
            "seasonal_amplitude_c": 0,
            "seasonal_peak_day": 1
        },
        "solar": {"latitude_deg": 0, "longitude_deg": 0},
        "rain_snow_threshold_c": 0,
        "unexpected": true
    })json");
    REQUIRE_THROWS_WITH(soilsim::loadWeatherProfile(unknownFieldPath),
                        Catch::Matchers::ContainsSubstring("unknown field"));
    std::filesystem::remove(unknownFieldPath);
}
