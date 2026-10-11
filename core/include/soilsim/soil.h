#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace soilsim
{
struct SoilParams
{
    std::string name;
    double cohesion;          // Pa
    double frictionAngle;     // rad
    double unitWeight;        // N/m^3
    double adhesion;          // Pa (soil-tool)
    double soilToolFriction;  // rad (delta)
    double bekkerKc;          // dimensionless coefficient placeholder
    double bekkerKphi;        // dimensionless coefficient placeholder
    double bekkerN;           // dimensionless exponent placeholder
    double angleOfRepose;     // rad
    double bulkingFactor;     // loose/in-situ volume ratio
    std::string source;
    bool verified;
};

std::vector<SoilParams> loadSoils(const std::filesystem::path& path);
} // namespace soilsim
