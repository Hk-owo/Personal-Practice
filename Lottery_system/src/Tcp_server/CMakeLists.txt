add_library(Tcp_server SHARED)

target_sources(Tcp_server
        PRIVATE
        Tcp_server.cpp
)
target_include_directories(Tcp_server
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(Tcp_server
        PRIVATE
        pch_if
        uring
        pthread
)