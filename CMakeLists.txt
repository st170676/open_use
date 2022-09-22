cmake_minimum_required(VERSION 3.23)
project(main C)

set(CMAKE_C_STANDARD 99)

find_package(open62541 REQUIRED COMPONENTS Events FullNamespace)

add_executable(main server.c)

target_link_libraries(main open62541::open62541)
