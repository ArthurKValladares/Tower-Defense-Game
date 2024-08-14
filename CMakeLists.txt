cmake_minimum_required(VERSION 3.25)
project(TD)

add_subdirectory(src)
add_subdirectory(vendor)

find_package(Vulkan REQUIRED)

target_link_libraries(src PRIVATE vendor ${Vulkan_LIBRARIES})