add_library(vendor INTERFACE)

add_subdirectory(Vulkan-Headers)
add_subdirectory(SDL)
add_subdirectory(vk-bootstrap)

target_link_libraries(vendor INTERFACE Vulkan::Headers SDL3::SDL3 vk-bootstrap::vk-bootstrap)