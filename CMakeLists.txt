cmake_minimum_required(VERSION 3.22)
project(object-tracker C)

set(CMAKE_C_STANDARD 23)
set(SRC_ROOT "${CMAKE_SOURCE_DIR}/src")
set(LIB_ROOT "${CMAKE_SOURCE_DIR}/lib")
set(INCLUDE_ROOT "${CMAKE_SOURCE_DIR}/include")

file(GLOB_RECURSE SRC_FILES "${SRC_ROOT}/*.c")
add_executable(object-tracker ${SRC_FILES})

find_package(glfw3 REQUIRED)

# Linking dynamic lib directory
target_link_directories(object-tracker
        PRIVATE ${LIB_ROOT}
)

# Including header files into the source codes
target_include_directories(object-tracker
        PRIVATE ${INCLUDE_ROOT}
)

# Linking dynamic library files into our executable file
target_link_libraries(object-tracker
        PRIVATE m
        PRIVATE glfw
        PRIVATE glad
        PRIVATE darknet
)

# Compile definations for executable file
target_compile_definitions(object-tracker
        PUBLIC TEST_MODEL
        PUBLIC WEBCAM_READER
)

# Target properties to specify lib directory according to runtime origin
set_target_properties(object-tracker PROPERTIES
        BUILD_RPATH              "\$ORIGIN/lib"
        ARCHIVE_OUTPUT_DIRECTORY "\$ORIGIN/lib"
        LIBRARY_OUTPUT_DIRECTORY "\$ORIGIN/lib"
)
