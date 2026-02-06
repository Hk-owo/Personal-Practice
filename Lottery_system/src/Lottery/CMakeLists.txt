add_library(Lottery SHARED)

target_sources(Lottery
        PRIVATE
        Lottery.cpp
)

target_include_directories(Lottery
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(Lottery
        PRIVATE
        pch_if
)