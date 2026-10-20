include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(soilsim_tests
    test_cutting.cpp
    test_excavate.cpp
    test_heightfield.cpp
    test_moisture.cpp
    test_soil.cpp
    test_smoke.cpp
    test_weather.cpp
)

target_link_libraries(soilsim_tests PRIVATE Catch2::Catch2WithMain soilsim_core)
target_compile_definitions(soilsim_tests PRIVATE
    SOILSIM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME soilsim_tests COMMAND soilsim_tests)
