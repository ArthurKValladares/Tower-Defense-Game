#include <iostream>
#include <print>

#include "renderer/vk_engine.h"
#include "map_editor/map.h"

auto main(int argc, char** argv) -> int {
    VkEngine engine;

    const std::optional<EngineInitError> init_result = engine.init();
    if (init_result.has_value()) {
        std::print("Could not initialize VkEngine\n");
        return -1;
    }

    MapLayout map = MapLayout::from_path("../maps/test_map.tdm");
    map.print();

    engine.run();

    engine.cleanup();
    
    return 0;
}