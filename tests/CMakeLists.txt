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

# Модульные тесты: домен, сервисы на репозиториях в памяти, фоновый поток, конфигурация.
# PostgreSQL для них не нужен.
add_executable(pvz_unit_tests
    domain_tests.cpp
    service_tests.cpp
    worker_tests.cpp
    config_tests.cpp
)
target_include_directories(pvz_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pvz_unit_tests PRIVATE pvz_service pvz_config GTest::gtest_main)
pvz_warnings(pvz_unit_tests)
gtest_discover_tests(pvz_unit_tests)

# Интеграционные тесты репозиториев PostgreSQL (пропускаются без PVZ_TEST_DB_NAME).
add_executable(pvz_pg_tests pg_repository_tests.cpp)
target_link_libraries(pvz_pg_tests PRIVATE pvz_postgres GTest::gtest_main)
target_compile_definitions(pvz_pg_tests PRIVATE PVZ_SQL_DIR="${PROJECT_SOURCE_DIR}/sql")
pvz_warnings(pvz_pg_tests)
gtest_discover_tests(pvz_pg_tests)
