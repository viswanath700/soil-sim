add_executable(dig_cli
    dig_cli.cpp
)

target_link_libraries(dig_cli PRIVATE soilsim_core)
target_link_libraries(dig_cli PRIVATE nlohmann_json::nlohmann_json)
