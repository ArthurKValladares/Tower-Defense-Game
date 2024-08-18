cmake_minimum_required(VERSION 3.25)
project(TD)

add_subdirectory(src)
add_subdirectory(vendor)

find_package(Vulkan REQUIRED)
target_link_libraries(src PRIVATE vendor ${Vulkan_LIBRARIES})

# Compile shaders
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

message("\n------- Compiling Shaders Step Start -------")
make_directory(${PROJECT_SOURCE_DIR}/build/shaders)
message(STATUS "GLSL Validador Path:\n" ${GLSL_VALIDATOR})
foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/build/shaders/${FILE_NAME}.spv")

  message(STATUS "Compiling Shader:\n" ${GLSL})
  message(STATUS "Compile Command:\n" ${GLSL_VALIDATOR} " " -V " " ${GLSL} " " -o " " ${SPIRV})

  execute_process(COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV})
endforeach(GLSL)
message("------- Compiling Shaders Step End -------\n")

file(COPY ${PROJECT_SOURCE_DIR}/assets DESTINATION ${PROJECT_SOURCE_DIR}/build)