#pragma once

#include <string>
#include <string_view>

namespace soilsim {
inline constexpr std::string_view kProjectName = "soilsim";
inline constexpr int kProjectVersionMajor = 0;
inline constexpr int kProjectVersionMinor = 1;
inline constexpr int kProjectVersionPatch = 0;

std::string projectName();
std::string projectVersion();
} // namespace soilsim
