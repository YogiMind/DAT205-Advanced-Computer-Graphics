cmake_minimum_required(VERSION 3.10)
project(project)

find_package(OpenMP)

file(GLOB_RECURSE SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.geom"
)

source_group("Shaders" FILES ${SHADERS})

add_executable(${PROJECT_NAME}
    main.cpp
    ply.cpp
    ${SHADERS}
)

target_link_libraries(${PROJECT_NAME} labhelper)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} OpenMP::OpenMP_CXX)
endif()

config_build_output()
