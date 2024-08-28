#pragma once

#include <filesystem>

enum class TileType : unsigned char {
    Invalid = 0,
    Path,
    Wall,
};

struct MapLayout {
    static MapLayout from_path(const std::filesystem::path& path);

    std::vector<std::vector<TileType>> tiles;
};