cmake_minimum_required(VERSION 3.15)
project(cias)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

set(DEFINATIONS
    -D_WIN32_WINNT=0x0600
)

set(DEPENDENCIES
    gdiplus.lib
)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFINATIONS})
target_link_libraries(${PROJECT_NAME} ${DEPENDENCIES})