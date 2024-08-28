#include "map.h"
#include "../defs.h"

#include <fstream>
#include <print>

MapLayout MapLayout::from_path(const std::filesystem::path& path) {
    std::fstream map_file;
	map_file.open(path, std::ios::in);
	if (!map_file) {
		std::print("Could not open file at: {}\n", path.string());
	}
    std::print("Loaded map at: {}\n", path.string());

    std::vector<std::vector<TileType>> tiles;
    std::string line;

    int num_rows;
    int num_cols = -1;
    while (std::getline(map_file, line)) {
        const int num_cols_line = line.length();
        if (num_cols == -1) {
            num_cols = num_cols_line;
        }
        M_Assert(num_cols == num_cols_line, "All lines in map must have the same number of tiles\n");

        std::vector<TileType> tiles_this_line;
        tiles_this_line.reserve(num_cols);

        for (const char& c : line) {
            switch(c) {
                case ' ': {
                    tiles_this_line.emplace_back(TileType::Path);
                    break;
                }
                case '#': {
                    tiles_this_line.emplace_back(TileType::Wall);
                    break;
                }
                default: {
                    M_Assert(false, "Unsupported tile type\n");
                    break;
                }
            }
        }

        tiles.emplace_back(std::move(tiles_this_line));
    }

    return MapLayout{
        tiles
    };
}