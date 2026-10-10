#include "soilsim/types.h"

#include <string>

namespace soilsim {
std::string projectName() {
    return std::string(kProjectName);
}

std::string projectVersion() {
    return std::to_string(kProjectVersionMajor) + "." +
           std::to_string(kProjectVersionMinor) + "." +
           std::to_string(kProjectVersionPatch);
}
} // namespace soilsim
