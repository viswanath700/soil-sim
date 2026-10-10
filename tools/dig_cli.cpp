#include "soilsim/types.h"

#include <iostream>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::cout << soilsim::projectName() << " " << soilsim::projectVersion() << '\n';
    return 0;
}
