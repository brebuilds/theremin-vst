add_executable(theremin_tests
    test_main.cpp
    test_calibration.cpp
)

target_include_directories(theremin_tests PRIVATE ../src)

target_sources(theremin_tests PRIVATE
    ../src/mapping/Calibration.cpp
)

target_link_libraries(theremin_tests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(theremin_tests)
