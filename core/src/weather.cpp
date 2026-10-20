#include "soilsim/weather.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
using json = nlohmann::json;
constexpr double kPi = 3.14159265358979323846;
constexpr double kSecondsPerDay = 86400.0;
constexpr double kMaximumPrecipitationRateMmPerHour = 100000.0;

void requireFinite(double value, const std::string& name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument(name + " must be finite");
    }
}

bool isLeapYear(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

unsigned daysInMonth(int year, unsigned month)
{
    constexpr unsigned kDaysPerMonth[] = {31, 28, 31, 30, 31, 30,
                                          31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return kDaysPerMonth[month - 1];
}

std::int64_t daysBeforeYear(int year)
{
    const std::int64_t precedingYears = static_cast<std::int64_t>(year) - 1;
    return 365 * precedingYears + precedingYears / 4 - precedingYears / 100 +
           precedingYears / 400;
}

void validateStartTime(const soilsim::WeatherStartTime& startTime)
{
    if (startTime.year < 1 || startTime.year > 9999) {
        throw std::invalid_argument("startTime.year must be between 1 and 9999");
    }
    if (startTime.month < 1 || startTime.month > 12) {
        throw std::invalid_argument("startTime.month must be between 1 and 12");
    }
    if (startTime.day < 1 || startTime.day > daysInMonth(startTime.year, startTime.month)) {
        throw std::invalid_argument("startTime.day is invalid for its Gregorian month");
    }
    requireFinite(startTime.secondsSinceMidnight, "startTime.secondsSinceMidnight");
    if (startTime.secondsSinceMidnight < 0.0 ||
        startTime.secondsSinceMidnight >= kSecondsPerDay) {
        throw std::invalid_argument("startTime.secondsSinceMidnight must be in [0, 86400)");
    }
}

void validateWeatherProfile(const soilsim::WeatherProfileData& profile)
{
    requireFinite(profile.meanAirTemperatureC, "meanAirTemperatureC");
    requireFinite(profile.dailyTemperatureAmplitudeC, "dailyTemperatureAmplitudeC");
    requireFinite(profile.seasonalTemperatureAmplitudeC, "seasonalTemperatureAmplitudeC");
    requireFinite(profile.latitudeDeg, "latitudeDeg");
    requireFinite(profile.longitudeDeg, "longitudeDeg");
    requireFinite(profile.rainSnowThresholdC, "rainSnowThresholdC");

    if (profile.meanAirTemperatureC < -100.0 || profile.meanAirTemperatureC > 100.0) {
        throw std::invalid_argument("meanAirTemperatureC must be between -100 and 100");
    }
    if (profile.dailyTemperatureAmplitudeC < 0.0 ||
        profile.seasonalTemperatureAmplitudeC < 0.0 ||
        profile.meanAirTemperatureC - profile.dailyTemperatureAmplitudeC -
                profile.seasonalTemperatureAmplitudeC <
            -100.0 ||
        profile.meanAirTemperatureC + profile.dailyTemperatureAmplitudeC +
                profile.seasonalTemperatureAmplitudeC >
            100.0) {
        throw std::invalid_argument(
            "temperature amplitudes must be nonnegative and keep air temperature in [-100, 100]");
    }
    if (profile.seasonalPeakDay < 1 || profile.seasonalPeakDay > 366) {
        throw std::invalid_argument("seasonalPeakDay must be between 1 and 366");
    }
    if (profile.latitudeDeg < -90.0 || profile.latitudeDeg > 90.0) {
        throw std::invalid_argument("latitudeDeg must be between -90 and 90");
    }
    if (profile.longitudeDeg < -180.0 || profile.longitudeDeg > 180.0) {
        throw std::invalid_argument("longitudeDeg must be between -180 and 180");
    }
    if (profile.rainSnowThresholdC < -100.0 || profile.rainSnowThresholdC > 100.0) {
        throw std::invalid_argument("rainSnowThresholdC must be between -100 and 100");
    }
    if (profile.source.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("weather profile source must not be empty");
    }

    for (std::size_t index = 0; index < profile.precipitationEvents.size(); ++index) {
        const auto& event = profile.precipitationEvents[index];
        const std::string context = "precipitationEvents[" + std::to_string(index) + "]";
        requireFinite(event.startSeconds, context + ".startSeconds");
        requireFinite(event.endSeconds, context + ".endSeconds");
        requireFinite(event.rateMmPerHour, context + ".rateMmPerHour");
        if (event.startSeconds < 0.0 || event.endSeconds <= event.startSeconds) {
            throw std::invalid_argument(context + " must have a nonnegative start and positive duration");
        }
        if (event.rateMmPerHour < 0.0 ||
            event.rateMmPerHour > kMaximumPrecipitationRateMmPerHour) {
            throw std::invalid_argument(context + ".rateMmPerHour is out of range");
        }
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

void requireOnlyKeys(const json& object, const std::initializer_list<const char*>& keys,
                    const std::string& context)
{
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        const bool known = std::any_of(keys.begin(), keys.end(), [&iterator](const char* key) {
            return iterator.key() == key;
        });
        if (!known) {
            throw std::runtime_error(context + " contains unknown field '" + iterator.key() + "'");
        }
    }
}

unsigned requiredDay(const json& object, const char* key, const std::string& context)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() ||
        (!iterator->is_number_integer() && !iterator->is_number_unsigned())) {
        throw std::runtime_error(context + " must contain integer field '" + key + "'");
    }
    const auto value = iterator->get<std::int64_t>();
    if (value < 1 || value > 366) {
        throw std::runtime_error(context + " field '" + key + "' must be between 1 and 366");
    }
    return static_cast<unsigned>(value);
}

std::int64_t startDayNumber(const soilsim::WeatherStartTime& startTime)
{
    std::int64_t result = daysBeforeYear(startTime.year);
    for (unsigned month = 1; month < startTime.month; ++month) {
        result += daysInMonth(startTime.year, month);
    }
    return result + static_cast<std::int64_t>(startTime.day) - 1;
}

struct CalendarInstant
{
    int year;
    unsigned dayOfYear;
    unsigned daysInYear;
    double secondsSinceMidnight;
};

CalendarInstant calendarInstant(const soilsim::WeatherStartTime& startTime,
                                double elapsedSeconds)
{
    requireFinite(elapsedSeconds, "elapsedSimulationSeconds");
    if (elapsedSeconds < 0.0) {
        throw std::invalid_argument("elapsedSimulationSeconds must be nonnegative");
    }
    constexpr std::int64_t kFirstDayOfYear10000 = 3652059;
    const double absoluteSeconds =
        static_cast<double>(startDayNumber(startTime)) * kSecondsPerDay +
        startTime.secondsSinceMidnight + elapsedSeconds;
    if (!std::isfinite(absoluteSeconds) ||
        absoluteSeconds >= static_cast<double>(kFirstDayOfYear10000) * kSecondsPerDay) {
        throw std::out_of_range("weather time exceeds the supported Gregorian calendar");
    }

    const auto dayNumber = static_cast<std::int64_t>(std::floor(absoluteSeconds / kSecondsPerDay));
    const double secondsSinceMidnight =
        absoluteSeconds - static_cast<double>(dayNumber) * kSecondsPerDay;
    int lowerYear = 1;
    int upperYear = 10000;
    while (lowerYear + 1 < upperYear) {
        const int middleYear = lowerYear + (upperYear - lowerYear) / 2;
        if (daysBeforeYear(middleYear) <= dayNumber) {
            lowerYear = middleYear;
        } else {
            upperYear = middleYear;
        }
    }

    const int year = lowerYear;
    const unsigned dayOfYear =
        static_cast<unsigned>(dayNumber - daysBeforeYear(year)) + 1U;
    return CalendarInstant{year, dayOfYear, isLeapYear(year) ? 366U : 365U,
                           secondsSinceMidnight};
}

double normalizeHour(double hour)
{
    hour = std::fmod(hour, 24.0);
    return hour < 0.0 ? hour + 24.0 : hour;
}

double solarDeclination(unsigned dayOfYear)
{
    return 0.409 * std::sin(2.0 * kPi * static_cast<double>(dayOfYear) / 365.0 - 1.39);
}

// Approximate daylight factor, not measured solar radiation and doesn't account for atmospheric effects
double calculateSolarFactor(const soilsim::WeatherProfileData& profile,
                            const CalendarInstant& instant)
{
    const double latitude = profile.latitudeDeg * kPi / 180.0;
    const double declination = solarDeclination(instant.dayOfYear);
    const double cosSunsetHourAngle = -std::tan(latitude) * std::tan(declination);
    if (cosSunsetHourAngle >= 1.0) {
        return 0.0;
    }

    const bool polarDay = cosSunsetHourAngle <= -1.0;
    const double sunsetHourAngle = polarDay ? kPi : std::acos(cosSunsetHourAngle);
    const double solarHour =
        normalizeHour(instant.secondsSinceMidnight / 3600.0 + profile.longitudeDeg / 15.0);
    const double hourAngle = (solarHour - 12.0) * kPi / 12.0;
    if (!polarDay && std::abs(hourAngle) > sunsetHourAngle) {
        return 0.0;
    }

    const double sinElevation = std::sin(latitude) * std::sin(declination) +
                                std::cos(latitude) * std::cos(declination) *
                                    std::cos(hourAngle);
    return std::clamp(sinElevation, 0.0, 1.0);
}
} // namespace

namespace soilsim
{
WeatherProfile::WeatherProfile(WeatherProfileData data) : data_(std::move(data))
{
    validateWeatherProfile(data_);
}

const WeatherProfileData& WeatherProfile::data() const noexcept
{
    return data_;
}

WeatherProfile loadWeatherProfile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open weather profile '" + path.string() + "'");
    }

    json document;
    try {
        input >> document;
    } catch (const json::parse_error& error) {
        throw std::runtime_error("Unable to parse weather profile '" + path.string() + "': " +
                                 std::string(error.what()));
    }
    if (!document.is_object()) {
        throw std::runtime_error("Weather profile must be a JSON object");
    }
    requireOnlyKeys(document, {"source", "verified", "air_temperature", "solar",
                               "rain_snow_threshold_c", "precipitation_events"},
                    "Weather profile");

    const auto airTemperature = document.find("air_temperature");
    if (airTemperature == document.end() || !airTemperature->is_object()) {
        throw std::runtime_error("Weather profile must contain object 'air_temperature'");
    }
    requireOnlyKeys(*airTemperature,
                    {"mean_c", "daily_amplitude_c", "seasonal_amplitude_c", "seasonal_peak_day"},
                    "air_temperature");

    const auto solar = document.find("solar");
    if (solar == document.end() || !solar->is_object()) {
        throw std::runtime_error("Weather profile must contain object 'solar'");
    }
    requireOnlyKeys(*solar, {"latitude_deg", "longitude_deg"}, "solar");

    WeatherProfileData data{};
    data.source = requiredString(document, "source", "Weather profile");
    data.verified = requiredBoolean(document, "verified", "Weather profile");
    data.meanAirTemperatureC =
        requiredNumber(*airTemperature, "mean_c", "air_temperature");
    data.dailyTemperatureAmplitudeC =
        requiredNumber(*airTemperature, "daily_amplitude_c", "air_temperature");
    data.seasonalTemperatureAmplitudeC =
        requiredNumber(*airTemperature, "seasonal_amplitude_c", "air_temperature");
    data.seasonalPeakDay = requiredDay(*airTemperature, "seasonal_peak_day", "air_temperature");
    data.latitudeDeg = requiredNumber(*solar, "latitude_deg", "solar");
    data.longitudeDeg = requiredNumber(*solar, "longitude_deg", "solar");
    data.rainSnowThresholdC = requiredNumber(document, "rain_snow_threshold_c", "Weather profile");

    const auto events = document.find("precipitation_events");
    if (events != document.end()) {
        if (!events->is_array()) {
            throw std::runtime_error("Weather profile field 'precipitation_events' must be an array");
        }
        data.precipitationEvents.reserve(events->size());
        for (std::size_t index = 0; index < events->size(); ++index) {
            const json& event = (*events)[index];
            const std::string context = "precipitation_events[" + std::to_string(index) + "]";
            if (!event.is_object()) {
                throw std::runtime_error(context + " must be an object");
            }
            requireOnlyKeys(event, {"start_seconds", "end_seconds", "rate_mm_per_h"}, context);
            data.precipitationEvents.push_back(
                PrecipitationEvent{requiredNumber(event, "start_seconds", context),
                                   requiredNumber(event, "end_seconds", context),
                                   requiredNumber(event, "rate_mm_per_h", context)});
        }
    }
    try {
        return WeatherProfile(std::move(data));
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error("Invalid weather profile '" + path.string() + "': " +
                                 std::string(error.what()));
    }
}

SimpleWeatherDriver::SimpleWeatherDriver(WeatherStartTime startTime, WeatherProfile profile)
    : startTime_(startTime), profile_(std::move(profile))
{
    validateStartTime(startTime_);
}

WeatherInputs SimpleWeatherDriver::at(double elapsedSimulationSeconds) const
{
    const CalendarInstant instant = calendarInstant(startTime_, elapsedSimulationSeconds);
    const WeatherProfileData& profile = profile_.data();
    const double daysInYearValue = static_cast<double>(instant.daysInYear);

    // Year’s position relative to the configured seasonal peak, expressed as an angle in radians
    const double seasonalPhase =
        2.0 * kPi * (static_cast<double>(instant.dayOfYear) -
                     static_cast<double>(profile.seasonalPeakDay)) /
        daysInYearValue;

    // Approximate solar hour, accounting for longitude and time of day, wrapped to [0, 24)
    const double solarHour =
        normalizeHour(instant.secondsSinceMidnight / 3600.0 + profile.longitudeDeg / 15.0);

    // Daily phase for diurnal temperature variation, expressed as an angle in radians
    const double dailyPhase = 2.0 * kPi * (solarHour - 15.0) / 24.0;

    const double airTemperatureC = profile.meanAirTemperatureC +
                                   profile.seasonalTemperatureAmplitudeC *
                                       std::cos(seasonalPhase) +
                                   profile.dailyTemperatureAmplitudeC * std::cos(dailyPhase);
    const double solarFactor = calculateSolarFactor(profile, instant);

    double totalPrecipitationRate = 0.0;
    for (const PrecipitationEvent& event : profile.precipitationEvents) {
        if (elapsedSimulationSeconds >= event.startSeconds &&
            elapsedSimulationSeconds < event.endSeconds) {
            totalPrecipitationRate += event.rateMmPerHour;
            if (!std::isfinite(totalPrecipitationRate)) {
                throw std::overflow_error("overlapping precipitation rates exceed numeric range");
            }
        }
    }

    const bool isSnow = airTemperatureC < profile.rainSnowThresholdC;
    const WeatherInputs result{airTemperatureC,
                               isSnow ? 0.0 : totalPrecipitationRate,
                               isSnow ? totalPrecipitationRate : 0.0,
                               solarFactor};
    if (!std::isfinite(result.airTemperatureC) || !std::isfinite(result.solarFactor) ||
        result.solarFactor < 0.0 || result.solarFactor > 1.0) {
        throw std::runtime_error("weather calculation produced an invalid result");
    }
    return result;
}
} // namespace soilsim
