cmake_minimum_required(VERSION 3.22)
project(object-tracker C)

set(CMAKE_C_STANDARD 23)
set(SRC_ROOT "${CMAKE_SOURCE_DIR}/src")
set(LIB_ROOT "${CMAKE_SOURCE_DIR}/lib")
set(INCLUDE_ROOT "${CMAKE_SOURCE_DIR}/include")

file(GLOB_RECURSE SRC_FILES "${SRC_ROOT}/*.c")

add_executable(object-tracker ${SRC_FILES})

find_package(glfw3 REQUIRED)

target_link_directories(object-tracker
        PRIVATE ${LIB_ROOT}
)
target_include_directories(object-tracker
        PRIVATE ${INCLUDE_ROOT}
)
target_link_libraries(object-tracker
        PRIVATE m
        PRIVATE glfw
        PRIVATE glad
        PRIVATE darknet
)
target_compile_definitions(object-tracker
        PUBLIC WEBCAM_READER
        PUBLIC TEST_REALNET_MODEL
)
