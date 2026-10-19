#include "soilsim/cutting.h"

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

void validateSoil(const soilsim::SoilParams& soil)
{
    requireFinite(soil.cohesion, "soil.cohesion");
    requireFinite(soil.frictionAngle, "soil.frictionAngle");
    requireFinite(soil.unitWeight, "soil.unitWeight");
    requireFinite(soil.adhesion, "soil.adhesion");
    requireFinite(soil.soilToolFriction, "soil.soilToolFriction");

    const double halfPi = kPi * 0.5;

    if (soil.cohesion < 0.0 || soil.adhesion < 0.0) {
        throw std::invalid_argument("soil cohesion and adhesion must be nonnegative");
    }
    if (soil.frictionAngle < 0.0 || soil.frictionAngle >= halfPi) {
        throw std::invalid_argument("soil.frictionAngle must be between 0 and pi/2 radians");
    }
    if (soil.unitWeight <= 0.0) {
        throw std::invalid_argument("soil.unitWeight must be positive");
    }
    if (soil.soilToolFriction < 0.0 || soil.soilToolFriction > halfPi) {
        throw std::invalid_argument(
            "soil.soilToolFriction must be between 0 and pi/2 radians");
    }
}

void validateCutInputs(const soilsim::SoilParams& soil, double depth, double width,
                       double rakeAngle, double surchargePa)
{
    validateSoil(soil);
    requireFinite(depth, "depth");
    requireFinite(width, "width");
    requireFinite(rakeAngle, "rakeAngle");
    requireFinite(surchargePa, "surchargePa");
    if (depth < 0.0) {
        throw std::invalid_argument("depth must be nonnegative");
    }
    if (width <= 0.0) {
        throw std::invalid_argument("width must be positive");
    }
    if (surchargePa < 0.0) {
        throw std::invalid_argument("surchargePa must be nonnegative");
    }
}
} // namespace

namespace soilsim
{
CuttingResult RankineCuttingModel::cut(const SoilParams& soil, double depth, double width,
                                       double rakeAngle, double surchargePa) const
{
    validateCutInputs(soil, depth, width, rakeAngle, surchargePa);

    const double halfPi = kPi * 0.5;
    const double kpAngle = (halfPi + soil.frictionAngle) * 0.5;
    const double tanKpAngle = std::tan(kpAngle);
    const double kp = tanKpAngle * tanKpAngle;
    const double force = width
                         * (0.5 * soil.unitWeight * depth * depth * kp
                          + 2.0 * soil.cohesion * depth * tanKpAngle // sqrt(kp) replaced with tanKpAngle
                          + surchargePa * depth * kp);
    if (!std::isfinite(force) || force < 0.0) {
        throw std::runtime_error("Rankine force calculation produced an invalid result");
    }
    return CuttingResult{force, force, 0.0};
}

double solveDepthForForce(const ICuttingModel& model, const SoilParams& soil,
                          double appliedForce, double width, double rakeAngle,
                          double surchargePa, double maxDepth, double tol)
{
    requireFinite(appliedForce, "appliedForce");
    requireFinite(maxDepth, "maxDepth");
    requireFinite(tol, "tol");
    if (maxDepth <= 0.0) {
        throw std::invalid_argument("maxDepth must be positive");
    }
    if (tol <= 0.0) {
        throw std::invalid_argument("tol must be positive");
    }

    if (appliedForce <= 0.0) {
        return 0.0;
    }
    const CuttingResult upper = model.cut(soil, maxDepth, width, rakeAngle, surchargePa);
    if (appliedForce >= upper.force) {
        // The applied force exceeds the force at max depth, return max depth.
        return maxDepth;
    }

    // Conduct a binary search to find the depth that produces the applied force.
    double lowerDepth = 0.0;
    double upperDepth = maxDepth;
    while (upperDepth - lowerDepth > tol) {
        const double middleDepth = lowerDepth + (upperDepth - lowerDepth) * 0.5;
        const double middleForce =
            model.cut(soil, middleDepth, width, rakeAngle, surchargePa).force;
        if (middleForce < appliedForce) {
            lowerDepth = middleDepth;
        } else {
            upperDepth = middleDepth;
        }
    }
    return lowerDepth + (upperDepth - lowerDepth) * 0.5;
}
} // namespace soilsim
