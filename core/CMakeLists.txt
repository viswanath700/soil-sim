add_library(soilsim_core STATIC
    src/version.cpp
)

target_include_directories(soilsim_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(soilsim_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(soilsim_core PRIVATE /W4 /WX)
else()
    target_compile_options(soilsim_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
