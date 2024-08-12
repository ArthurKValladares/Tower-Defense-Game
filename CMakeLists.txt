cmake_minimum_required(VERSION 3.25)
project(TD)

add_subdirectory(src)
add_subdirectory(vendor)

target_link_libraries(src PRIVATE vendor)