#pragma once

#include "../defs.h"
#include "../geometry/cube.h"
#include "tile_types.h"

#include <filesystem>

struct MapLayout {
    static MapLayout from_path(const std::filesystem::path& path);

    void print() const;

    std::vector<std::vector<TileType>> tiles;
    std::vector<std::pair<int, int>> entry_points;
};

struct Map {
    Map() {}
    Map(VkEngine* engine, MapLayout& layout);

    void clear() {
        map_cubes.clear();
        spawn_cubes.clear();
        outer_cubes.clear();
        margins.clear();
        
        core_model.reset();
    }
    
    void draw(const glm::mat4& top_matrix, DrawContext& ctx) const;

    std::vector<std::vector<std::unique_ptr<Cube>>> map_cubes;
    std::vector<std::unique_ptr<Cube>> spawn_cubes;
    std::vector<std::unique_ptr<Cube>> outer_cubes;
    std::vector<std::unique_ptr<Cube>> margins;
    std::unique_ptr<Cube> core_model;
};