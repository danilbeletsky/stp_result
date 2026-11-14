cmake_minimum_required(VERSION 3.20)
project(pvz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(PVZ_BUILD_TESTS "Собирать тесты (GoogleTest)" ON)

find_package(libpqxx REQUIRED CONFIG)

function(pvz_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

set(PVZ_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- domain: сущности предметной области
add_library(pvz_domain STATIC
    src/domain/ParcelSize.cpp
    src/domain/Customer.cpp
    src/domain/Cell.cpp
    src/domain/Order.cpp
)
target_include_directories(pvz_domain PUBLIC ${PVZ_SRC})
pvz_warnings(pvz_domain)

# --- config: настройки из файла и переменных окружения
add_library(pvz_config STATIC
    src/config/Config.cpp
)
target_link_libraries(pvz_config PUBLIC pvz_domain)
pvz_warnings(pvz_config)

# --- service: бизнес-логика (работает с интерфейсами из src/repository)
add_library(pvz_service STATIC
    src/service/CustomerService.cpp
    src/service/CellService.cpp
    src/service/OrderService.cpp
)
target_link_libraries(pvz_service PUBLIC pvz_domain)
pvz_warnings(pvz_service)

# --- repository/postgres: реализация репозиториев на PostgreSQL (libpqxx)
add_library(pvz_postgres STATIC
    src/repository/postgres/PgDatabase.cpp
    src/repository/postgres/PgCustomerRepository.cpp
    src/repository/postgres/PgCellRepository.cpp
    src/repository/postgres/PgOrderRepository.cpp
)
target_link_libraries(pvz_postgres PUBLIC pvz_domain pvz_config libpqxx::pqxx)
pvz_warnings(pvz_postgres)

if(PVZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
