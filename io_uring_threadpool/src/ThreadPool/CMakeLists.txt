add_library(ThreadPool SHARED)
target_sources(ThreadPool
        PRIVATE
        ThreadPool.cpp
)
target_include_directories(ThreadPool
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(ThreadPool
        PRIVATE
        uring
        pthread
)
target_precompile_headers(ThreadPool
        PRIVATE
        ${PROJECT_SOURCE_DIR}/pch/pch.h
)