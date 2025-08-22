cmake_minimum_required(VERSION 3.22.1)
project(object-tracker VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB_RECURSE SOURCE_FILES "src/*.c")

add_executable(object-tracker ${SOURCE_FILES})

target_link_libraries(object-tracker m)

target_compile_definitions(object-tracker
        PUBLIC WEBCAM_READER)
target_include_directories(object-tracker
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
