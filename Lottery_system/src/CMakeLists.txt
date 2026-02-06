add_subdirectory(Lottery)
add_subdirectory(Tcp_server)

add_executable(${PROJECT_NAME}
        main.cpp
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        pch_if
        Tcp_server
        Lottery
)

