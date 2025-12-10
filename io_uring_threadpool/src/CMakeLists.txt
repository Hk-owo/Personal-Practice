file(
        GLOB SRC_CPP
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

add_executable(${PROJECT_NAME}
    ${SRC_CPP}
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        uring
        pthread
)
