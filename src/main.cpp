#include <iostream>
#include <print>

#include "vk_engine.h"

auto main(int argc, char** argv) -> int {
    VkEngine engine;

    const std::optional<EngineInitError> init_result = engine.init();
    if (init_result.has_value()) {
        std::print("Could not initialize VkEngine\n");
        return -1;
    }

    engine.run();

    engine.cleanup();
    
    return 0;
}