#pragma once

#include "../defs.h"
#include "../geometry/cube.h"

#include <filesystem>

enum class TileType : unsigned char {
    Invalid = 0,
    Path,
    Wall,
    Core,
};

// TODO: Auto-generate this function, and a reversed one later
static char tile_type_to_char(TileType ty) {
    switch (ty) {
        case TileType::Path: {
            return ' ';
        }
        case TileType::Wall: {
            return '#';
        }
        case TileType::Core: {
            return 'O';
        }
        case TileType::Invalid: {
            M_Assert(false, "Tile type not supported");
            return '\0';
        }
    }
}

struct MapLayout {
    static MapLayout from_path(const std::filesystem::path& path);

    void print() const;

    std::vector<std::vector<TileType>> tiles;    
};

struct Map {
    Map() {}
    Map(VkEngine* engine, MapLayout& layout);

    void clear() {
        map_cubes.clear();
        core_model.reset();
    }
    
    void draw(const glm::mat4& top_matrix, DrawContext& ctx) const;

    std::vector<std::vector<std::unique_ptr<Cube>>> map_cubes;
    std::unique_ptr<Cube> core_model;
};