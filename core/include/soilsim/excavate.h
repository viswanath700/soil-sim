#pragma once

#include "soilsim/cutting.h"
#include "soilsim/heightfield.h"

namespace soilsim
{
struct BucketPose
{
    double x;
    double y;
    double z;
    double yaw;
    double rakeAngle;
    double width;
};

struct DigStepResult
{
    double time;
    double depth;
    double force;
    double volumeRemoved;
    double bucketFill;
};

struct BucketState
{
    double capacity;
    double carriedVolume;
};

DigStepResult digStep(Heightfield& terrain, BucketState& bucket, const SoilParams& soil,
                      const ICuttingModel& model, const BucketPose& pose, double appliedForce,
                      double dt);

void dump(Heightfield& terrain, BucketState& bucket, const SoilParams& soil, double x, double y);
} // namespace soilsim
