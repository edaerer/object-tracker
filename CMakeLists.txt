cmake_minimum_required(VERSION 3.22)
project(object-tracker C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE LIBS_SOD_FILES "${CMAKE_SOURCE_DIR}/libs/sod/*.c")

add_executable(object-tracker ${SRC_FILES})
add_library(sod STATIC ${LIBS_SOD_FILES})

include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/libs/sod
)
target_link_libraries(object-tracker
        PUBLIC sod
        PUBLIC m
)
target_compile_definitions(sod
        PUBLIC SOD_ENABLE_NET_TRAIN
)
target_compile_definitions(object-tracker
        PUBLIC WEBCAM_READER
        PUBLIC TEST_REALNET_MODEL
)
target_include_directories(object-tracker
        PUBLIC ${CMAKE_SOURCE_DIR}/include
        PUBLIC ${CMAKE_SOURCE_DIR}/libs/sod
)
