#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace soilsim
{
struct WeatherInputs
{
    double airTemperatureC;
    double rainRateMmPerHour;
    double snowfallRateMmPerHour;
    double solarFactor;
};

struct PrecipitationEvent
{
    double startSeconds;
    double endSeconds;
    double rateMmPerHour;
};

struct WeatherProfileData
{
    double meanAirTemperatureC;
    double dailyTemperatureAmplitudeC;
    double seasonalTemperatureAmplitudeC;
    unsigned seasonalPeakDay;
    double latitudeDeg;
    double longitudeDeg;
    double rainSnowThresholdC;
    std::vector<PrecipitationEvent> precipitationEvents;
    std::string source;
    bool verified;
};

class WeatherProfile
{
public:
    explicit WeatherProfile(WeatherProfileData data);

    const WeatherProfileData& data() const noexcept;

private:
    WeatherProfileData data_;
};

WeatherProfile loadWeatherProfile(const std::filesystem::path& path);

struct WeatherStartTime
{
    int year;
    unsigned month;
    unsigned day;
    double secondsSinceMidnight;
};

class IWeatherDriver
{
public:
    virtual ~IWeatherDriver() = default;
    virtual WeatherInputs at(double elapsedSimulationSeconds) const = 0;
};

class SimpleWeatherDriver final : public IWeatherDriver
{
public:
    SimpleWeatherDriver(WeatherStartTime startTime, WeatherProfile profile);

    WeatherInputs at(double elapsedSimulationSeconds) const override;

private:
    WeatherStartTime startTime_;
    WeatherProfile profile_;
};
} // namespace soilsim
