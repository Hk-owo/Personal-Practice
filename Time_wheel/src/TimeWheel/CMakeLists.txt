add_library(TimeWheelLib)

target_sources(TimeWheelLib PRIVATE TimeWheel.cpp)

target_include_directories(TimeWheelLib PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(TimeWheelLib PRIVATE pch_if uring)