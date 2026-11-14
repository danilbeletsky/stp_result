# GoogleTest: сначала ищем установленный в системе, иначе скачиваем.
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

# Модульные тесты: домен, сервисы на репозиториях в памяти, конфигурация.
# PostgreSQL для них не нужен.
add_executable(pvz_unit_tests
    domain_tests.cpp
    service_tests.cpp
    config_tests.cpp
)
target_include_directories(pvz_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pvz_unit_tests PRIVATE pvz_service pvz_config GTest::gtest_main)
pvz_warnings(pvz_unit_tests)
gtest_discover_tests(pvz_unit_tests)
