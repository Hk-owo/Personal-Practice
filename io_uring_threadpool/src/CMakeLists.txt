file(
        GLOB SRC_CPP
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

add_executable(${PROJECT_NAME}
    ${SRC_CPP}
#        main.cpp
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        ThreadPool
)

add_subdirectory(ThreadPool)
