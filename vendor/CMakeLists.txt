add_library(vendor INTERFACE)

add_subdirectory(Vulkan-Headers)
add_subdirectory(SDL)

target_link_libraries(vendor INTERFACE Vulkan::Headers SDL3::SDL3)