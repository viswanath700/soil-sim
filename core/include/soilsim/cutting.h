#pragma once

#include "soilsim/soil.h"

namespace soilsim
{
struct CuttingResult
{
    double force;           // N
    double forceHorizontal; // N
    double forceVertical;   // N
};

class ICuttingModel
{
public:
    virtual CuttingResult cut(const SoilParams& soil, double depth, double width,
                              double rakeAngle, double surchargePa) const = 0;
    virtual ~ICuttingModel() = default;
};

class RankineCuttingModel final : public ICuttingModel
{
public:
    CuttingResult cut(const SoilParams& soil, double depth, double width, double rakeAngle,
                      double surchargePa) const override;
};

// TODO: ReeceCuttingModel

// Returns the equality depth when bracketed, or maxDepth when the applied force is
// greater than the resistance at maxDepth. All lengths are metres, forces are newtons,
// pressure is pascals, and angles are radians.
double solveDepthForForce(const ICuttingModel& model, const SoilParams& soil,
                          double appliedForce, double width, double rakeAngle,
                          double surchargePa, double maxDepth, double tol = 1e-6);
} // namespace soilsim
