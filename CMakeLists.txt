cmake_minimum_required(VERSION 3.22.1)
project(object-tracker VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB_RECURSE SRC_FILES "src/*.c")
file(GLOB_RECURSE LIBS_SOD_FILES "libs/sod/*.c")

add_executable(object-tracker ${SRC_FILES})
add_library(sod ${LIBS_SOD_FILES})

target_link_libraries(object-tracker
        sod m
)
target_compile_definitions(object-tracker
        PUBLIC WEBCAM_READER
        PUBLIC TEST_REALNET_MODEL
)
target_include_directories(object-tracker
        PUBLIC include
        PUBLIC libs/sod
)
