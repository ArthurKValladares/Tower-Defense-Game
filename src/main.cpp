#include <iostream>
#include <print>

#include "vk_engine.h"

auto main() -> int {
    VkEngine engine;

    const std::optional<EngineInitError> init_result = engine.init();
    if (init_result.has_value()) {
        std::print("Could not initialize VkEngine");
        return -1;
    }

    engine.run();

    engine.cleanup();
    
    return 0;
}