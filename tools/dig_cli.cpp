#include "soilsim/excavate.h"
#include "soilsim/soil.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using json = nlohmann::json;

struct PathPoint
{
    double x;
    double y;
    double z;
    double yaw;
    double rakeAngle;
};

struct Scenario
{
    std::string soilName;
    double force;
    std::size_t columns;
    std::size_t rows;
    double cellSize;
    double originX;
    double originY;
    double initialHeight;
    double bucketCapacity;
    double bucketWidth;
    double dt;
    std::vector<PathPoint> path;
};

struct Options
{
    std::filesystem::path soilFile = "data/soils.json";
    std::filesystem::path scenarioFile;
    std::filesystem::path pathFile;
    std::filesystem::path outputDirectory = "out/cli";
    std::string soilName;
    double force = 0.0;
    double cellSize = 0.0;
};

double requiredNumber(const json& object, const char* key, const std::string& context)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_number()) {
        throw std::runtime_error(context + " must contain numeric field '" + key + "'");
    }
    return iterator->get<double>();
}

std::string requiredString(const json& object, const char* key, const std::string& context)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_string()) {
        throw std::runtime_error(context + " must contain string field '" + key + "'");
    }
    return iterator->get<std::string>();
}

std::vector<PathPoint> loadPath(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open path file '" + path.string() + "'");
    }
    std::vector<PathPoint> pathPoints;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        PathPoint point{};
        if (!(row >> point.x >> point.y >> point.z >> point.yaw >> point.rakeAngle)) {
            throw std::runtime_error("Invalid path row " + std::to_string(lineNumber) +
                                     " in '" + path.string() + "'");
        }
        pathPoints.push_back(point);
    }
    if (pathPoints.empty()) {
        throw std::runtime_error("Path file contains no points: '" + path.string() + "'");
    }
    return pathPoints;
}

Scenario loadScenario(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open scenario file '" + path.string() + "'");
    }
    json document;
    input >> document;
    const std::string context = "Scenario";
    const json& grid = document.at("grid");
    const json& bucket = document.at("bucket");
    Scenario scenario{
        requiredString(document, "soil", context),
        requiredNumber(document, "force_N", context),
        static_cast<std::size_t>(requiredNumber(grid, "columns", "Scenario grid")),
        static_cast<std::size_t>(requiredNumber(grid, "rows", "Scenario grid")),
        requiredNumber(grid, "cell_size_m", "Scenario grid"),
        requiredNumber(grid, "origin_x_m", "Scenario grid"),
        requiredNumber(grid, "origin_y_m", "Scenario grid"),
        requiredNumber(grid, "initial_height_m", "Scenario grid"),
        requiredNumber(bucket, "capacity_m3_loose", "Scenario bucket"),
        requiredNumber(bucket, "width_m", "Scenario bucket"),
        requiredNumber(document, "dt_s", context),
        {}};
    const auto& points = document.at("path");
    if (!points.is_array() || points.empty()) {
        throw std::runtime_error("Scenario path must be a nonempty array");
    }
    for (const auto& point : points) {
        scenario.path.push_back(PathPoint{
            point.at("x_m").get<double>(), point.at("y_m").get<double>(),
            point.at("z_m").get<double>(), point.value("yaw_rad", 0.0),
            point.value("rake_angle_rad", 0.0)});
    }
    return scenario;
}

std::string nextValue(int& index, int argc, char** argv, const std::string& option)
{
    if (index + 1 >= argc) {
        throw std::runtime_error("Missing value for " + option);
    }
    return argv[++index];
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--scenario") {
            options.scenarioFile = nextValue(index, argc, argv, argument);
        } else if (argument == "--soil") {
            options.soilName = nextValue(index, argc, argv, argument);
        } else if (argument == "--force") {
            options.force = std::stod(nextValue(index, argc, argv, argument));
        } else if (argument == "--path") {
            options.pathFile = nextValue(index, argc, argv, argument);
        } else if (argument == "--cell") {
            options.cellSize = std::stod(nextValue(index, argc, argv, argument));
        } else if (argument == "--out") {
            options.outputDirectory = nextValue(index, argc, argv, argument);
        } else if (argument == "--soil-file") {
            options.soilFile = nextValue(index, argc, argv, argument);
        } else if (argument == "--help" || argument == "-h") {
            std::cout << "Usage: dig_cli --scenario FILE --out DIR\n"
                         "   or: dig_cli --soil NAME --force N --path FILE --cell SIZE --out DIR\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown option '" + argument + "'");
        }
    }
    return options;
}

void writeStepsCsv(const std::filesystem::path& path, const std::vector<soilsim::DigStepResult>& steps)
{
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Unable to write '" + path.string() + "'");
    }
    output << "time_s,depth_m,force_N,volume_removed_in_situ_m3,bucket_fill_m3_loose\n";
    output << std::setprecision(17);
    for (const auto& step : steps) {
        output << step.time << ',' << step.depth << ',' << step.force << ','
               << step.volumeRemoved << ',' << step.bucketFill << '\n';
    }
}

void writeHeightfieldCsv(const std::filesystem::path& path, const soilsim::Heightfield& terrain)
{
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Unable to write '" + path.string() + "'");
    }
    output << "x_m,y_m,height_m\n" << std::setprecision(17);
    for (std::size_t row = 0; row < terrain.rows(); ++row) {
        for (std::size_t column = 0; column < terrain.columns(); ++column) {
            output << terrain.originX() + (static_cast<double>(column) + 0.5) * terrain.cellSize()
                   << ',' << terrain.originY() + (static_cast<double>(row) + 0.5) * terrain.cellSize()
                   << ',' << terrain.at(column, row) << '\n';
        }
    }
}

Scenario makeDirectScenario(const Options& options)
{
    if (options.soilName.empty() || options.pathFile.empty() || options.cellSize <= 0.0) {
        throw std::runtime_error(
            "Direct mode requires --soil NAME, --force N, --path FILE, and --cell SIZE");
    }
    const auto path = loadPath(options.pathFile);
    Scenario scenario{options.soilName, options.force, 40, 40, options.cellSize, 0.0, 0.0,
                      1.0, 10.0, 0.5, 0.1, path};
    return scenario;
}
} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parseOptions(argc, argv);
        const Scenario scenario =
            options.scenarioFile.empty() ? makeDirectScenario(options)
                                         : loadScenario(options.scenarioFile);
        const auto soils = soilsim::loadSoils(options.soilFile);
        const auto soilIterator =
            std::find_if(soils.begin(), soils.end(), [&](const auto& soil) {
                return soil.name == scenario.soilName;
            });
        if (soilIterator == soils.end()) {
            throw std::runtime_error("Unknown soil preset '" + scenario.soilName + "'");
        }
        if (scenario.columns == 0 || scenario.rows == 0 || scenario.cellSize <= 0.0 ||
            scenario.bucketCapacity <= 0.0 || scenario.bucketWidth <= 0.0 ||
            scenario.dt <= 0.0 || scenario.path.empty()) {
            throw std::runtime_error("Scenario contains invalid grid, bucket, timestep, or path values");
        }

        std::filesystem::create_directories(options.outputDirectory);
        soilsim::Heightfield terrain(scenario.columns, scenario.rows, scenario.cellSize,
                                     scenario.originX, scenario.originY, scenario.initialHeight);
        soilsim::BucketState bucket{scenario.bucketCapacity, 0.0};
        const soilsim::RankineCuttingModel model;
        std::vector<soilsim::DigStepResult> steps;
        steps.reserve(scenario.path.size());
        const auto start = std::chrono::steady_clock::now();
        for (const auto& point : scenario.path) {
            const soilsim::BucketPose pose{point.x, point.y, point.z, point.yaw,
                                           point.rakeAngle, scenario.bucketWidth};
            auto result = soilsim::digStep(terrain, bucket, *soilIterator, model, pose,
                                           scenario.force, scenario.dt);
            result.time = static_cast<double>(steps.size() + 1) * scenario.dt;
            steps.push_back(result);
        }
        const auto finish = std::chrono::steady_clock::now();
        const auto& finalPoint = scenario.path.back();
        const double dumpX =
            finalPoint.x - std::sin(finalPoint.yaw) * scenario.bucketWidth;
        const double dumpY =
            finalPoint.y + std::cos(finalPoint.yaw) * scenario.bucketWidth;
        soilsim::dump(terrain, bucket, *soilIterator, dumpX, dumpY);
        const double elapsedSeconds =
            std::chrono::duration<double>(finish - start).count();
        const double stepsPerSecond =
            elapsedSeconds > 0.0 ? static_cast<double>(steps.size()) / elapsedSeconds : 0.0;

        writeStepsCsv(options.outputDirectory / "steps.csv", steps);
        writeHeightfieldCsv(options.outputDirectory / "heightfield_final.csv", terrain);
        terrain.writeBinary(options.outputDirectory / "heightfield_final.bin");
        std::cout << "soil=" << scenario.soilName << " steps=" << steps.size()
                  << " final_bucket_fill_m3_loose=" << std::setprecision(17)
                  << bucket.carriedVolume << " steps_per_second=" << stepsPerSecond << '\n';
        std::ofstream benchmark("docs/benchmarks.md", std::ios::app);
        if (benchmark) {
            benchmark << "- " << scenario.soilName << ": " << stepsPerSecond
                      << " dig steps/s (" << steps.size() << " steps)\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "dig_cli error: " << error.what() << '\n';
        return 1;
    }
}
