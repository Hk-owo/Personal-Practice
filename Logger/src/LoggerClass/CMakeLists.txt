add_library(LoggerLib SHARED)

find_package(spdlog 1.12 REQUIRED)

target_include_directories(LoggerLib PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(LoggerLib PRIVATE LoggerClass.cpp)
target_link_libraries(LoggerLib PUBLIC spdlog::spdlog)