#pragma once

#include "../defs.h"

#include <filesystem>

enum class TileType : unsigned char {
    Invalid = 0,
    Path,
    Wall,
};

static char tile_type_to_char(TileType ty) {
    switch (ty) {
        case TileType::Path: {
            return ' ';
        }
        case TileType::Wall: {
            return '#';
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