#include "soilsim/excavate.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
constexpr double kPi = 3.14159265358979323846;

void requireFinite(double value, const char* name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void validateBucket(const soilsim::BucketState& bucket)
{
    requireFinite(bucket.capacity, "bucket.capacity");
    requireFinite(bucket.carriedVolume, "bucket.carriedVolume");
    if (bucket.capacity <= 0.0 || bucket.carriedVolume < 0.0 ||
        bucket.carriedVolume > bucket.capacity) {
        throw std::invalid_argument("bucket capacity and carried volume are invalid");
    }
}

void validatePose(const soilsim::BucketPose& pose)
{
    requireFinite(pose.x, "pose.x");
    requireFinite(pose.y, "pose.y");
    requireFinite(pose.z, "pose.z");
    requireFinite(pose.yaw, "pose.yaw");
    requireFinite(pose.rakeAngle, "pose.rakeAngle");
    requireFinite(pose.width, "pose.width");
    if (pose.width <= 0.0) {
        throw std::invalid_argument("pose.width must be positive");
    }
}
} // namespace

namespace soilsim
{
DigStepResult digStep(Heightfield& terrain, BucketState& bucket, const SoilParams& soil,
                      const ICuttingModel& model, const BucketPose& pose, double appliedForce,
                      double dt)
{
    validateBucket(bucket);
    validatePose(pose);
    requireFinite(appliedForce, "appliedForce");
    requireFinite(dt, "dt");
    if (dt < 0.0) {
        throw std::invalid_argument("dt must be nonnegative");
    }
    requireFinite(soil.bulkingFactor, "soil.bulkingFactor");
    if (soil.bulkingFactor <= 0.0) {
        throw std::invalid_argument("soil.bulkingFactor must be positive");
    }

    const double halfWidth = pose.width * 0.5;
    const double cosine = std::cos(pose.yaw);
    const double sine = std::sin(pose.yaw);
    double removedVolume = 0.0;
    double maximumDepth = 0.0;
    double actualForce = 0.0;
    const double availableLoose = bucket.capacity - bucket.carriedVolume;
    const double availableInSitu = availableLoose / soil.bulkingFactor;
    double remainingInSitu = availableInSitu;

    for (std::size_t row = 0; row < terrain.rows() && remainingInSitu > 0.0; ++row) {
        for (std::size_t column = 0; column < terrain.columns() && remainingInSitu > 0.0;
             ++column) {
            const double centerX =
                terrain.originX() + (static_cast<double>(column) + 0.5) * terrain.cellSize();
            const double centerY =
                terrain.originY() + (static_cast<double>(row) + 0.5) * terrain.cellSize();
            const double localX = cosine * (centerX - pose.x) + sine * (centerY - pose.y);
            const double localY = -sine * (centerX - pose.x) + cosine * (centerY - pose.y);
            if (std::abs(localX) > halfWidth || std::abs(localY) > halfWidth) {
                continue;
            }

            const double commandedDepth = std::max(0.0, terrain.at(column, row) - pose.z);
            if (commandedDepth == 0.0) {
                continue;
            }
            const double solvedDepth = solveDepthForForce(
                model, soil, appliedForce, pose.width, pose.rakeAngle, 0.0, commandedDepth);
            const double cellArea = terrain.cellSize() * terrain.cellSize();
            const double removableDepth = std::min(solvedDepth, remainingInSitu / cellArea);
            if (removableDepth <= 0.0) {
                continue;
            }
            terrain.at(column, row) -= removableDepth;
            const double cellVolume = removableDepth * cellArea;
            removedVolume += cellVolume;
            remainingInSitu -= cellVolume;
            maximumDepth = std::max(maximumDepth, removableDepth);
            actualForce = model.cut(soil, removableDepth, pose.width, pose.rakeAngle, 0.0).force;
        }
    }

    const double looseRemoved = removedVolume * soil.bulkingFactor;
    bucket.carriedVolume += looseRemoved;
    return DigStepResult{dt, maximumDepth, actualForce, removedVolume, bucket.carriedVolume};
}

void dump(Heightfield& terrain, BucketState& bucket, const SoilParams& soil, double x, double y)
{
    validateBucket(bucket);
    requireFinite(x, "x");
    requireFinite(y, "y");
    requireFinite(soil.angleOfRepose, "soil.angleOfRepose");
    requireFinite(soil.bulkingFactor, "soil.bulkingFactor");
    if (soil.angleOfRepose <= 0.0 || soil.angleOfRepose >= kPi / 2.0) {
        throw std::invalid_argument("soil.angleOfRepose must be between 0 and pi/2 radians");
    }
    if (soil.bulkingFactor <= 0.0) {
        throw std::invalid_argument("soil.bulkingFactor must be positive");
    }
    if (bucket.carriedVolume == 0.0) {
        return;
    }

    const double volume = bucket.carriedVolume / soil.bulkingFactor;
    const double tangent = std::tan(soil.angleOfRepose);
    const double cellArea = terrain.cellSize() * terrain.cellSize();
    double peakHeight = 1.0;
    const auto moundVolume = [&](double height) {
        double result = 0.0;
        for (std::size_t row = 0; row < terrain.rows(); ++row) {
            for (std::size_t column = 0; column < terrain.columns(); ++column) {
                const double centerX = terrain.originX() +
                                       (static_cast<double>(column) + 0.5) * terrain.cellSize();
                const double centerY = terrain.originY() +
                                       (static_cast<double>(row) + 0.5) * terrain.cellSize();
                const double distance = std::hypot(centerX - x, centerY - y);
                result += std::max(0.0, height - tangent * distance) * cellArea;
            }
        }
        return result;
    };
    while (moundVolume(peakHeight) < volume) {
        peakHeight *= 2.0;
        if (!std::isfinite(peakHeight)) {
            throw std::runtime_error("dump volume cannot fit in the heightfield");
        }
    }
    double lowerHeight = 0.0;
    double upperHeight = peakHeight;
    for (int iteration = 0; iteration < 80; ++iteration) {
        const double middleHeight = lowerHeight + (upperHeight - lowerHeight) * 0.5;
        if (moundVolume(middleHeight) < volume) {
            lowerHeight = middleHeight;
        } else {
            upperHeight = middleHeight;
        }
    }
    const double solvedPeakHeight = lowerHeight + (upperHeight - lowerHeight) * 0.5;
    for (std::size_t row = 0; row < terrain.rows(); ++row) {
        for (std::size_t column = 0; column < terrain.columns(); ++column) {
            const double centerX = terrain.originX() +
                                   (static_cast<double>(column) + 0.5) * terrain.cellSize();
            const double centerY = terrain.originY() +
                                   (static_cast<double>(row) + 0.5) * terrain.cellSize();
            const double distance = std::hypot(centerX - x, centerY - y);
            terrain.at(column, row) += std::max(0.0, solvedPeakHeight - tangent * distance);
        }
    }
    bucket.carriedVolume = 0.0;
}
} // namespace soilsim
