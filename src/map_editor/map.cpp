#include "map.h"

#include "../renderer/vk_engine.h"

#include <fstream>
#include <print>
#include <unordered_map>
#include <utility>

struct RowColPair {
    int r;
    int c;
};

bool operator==(const RowColPair &a, const RowColPair &b) { 
    return a.r == b.r && a.c == b.c;
}

template<> struct std::hash<RowColPair> {
    std::size_t operator()(RowColPair const& s) const noexcept {
        std::size_t h1 = std::hash<int>{}(s.r);
        std::size_t h2 = std::hash<int>{}(s.c);
        return h1 ^ (h2 << 1);
    }
};

MapLayout MapLayout::from_path(const std::filesystem::path& path) {
    std::fstream map_file;
	map_file.open(path, std::ios::in);
	if (!map_file) {
		std::print("Could not open file at: {}\n", path.string());
	}
    std::print("Loaded map at: {}\n", path.string());

    std::vector<std::vector<TileType>> tiles;
    std::vector<std::pair<int, int>> entry_points;
    std::string line;

    int core_row = -1;
    int core_col = -1;

    int num_cols = -1;
    int row = 0;
    while (std::getline(map_file, line)) {
        const int num_cols_line = line.length();
        if (num_cols == -1) {
            num_cols = num_cols_line;
        }
        M_Assert(num_cols == num_cols_line, "All lines in map must have the same number of tiles.");

        std::vector<TileType> tiles_this_line;
        tiles_this_line.reserve(num_cols);

        for (int col = 0; col < num_cols; ++col) {
            const char& c = line[col];
            const TileType ty = char_to_tile_type(c);
            tiles_this_line.emplace_back(ty);
            if (ty == TileType::Core) {
                M_Assert(core_row == -1 && core_col == -1, "Maps can only have one core.");
                core_row = row;
                core_col = col;
            }
        }

        tiles.emplace_back(std::move(tiles_this_line));
        ++row;
    }
    const int num_rows = row;
    M_Assert(core_row != -1 && core_col != -1, "Map must have a core.");

    // DFS to validate that all paths from core reach the edge of the map

    // RowColPair helper functions
    const auto is_in_bounds = [&](const RowColPair& tile) {
        return tile.r >= 0 && tile.c >= 0 && tile.r <= num_rows - 1 && tile.c <= num_cols - 1;
    };
    const auto is_on_edge = [&](const RowColPair& tile) {
        return tile.r == 0 || tile.c == 0 || tile.r == num_rows - 1 || tile.c == num_cols - 1;
    };
    const auto get_tile_type = [&](const RowColPair& tile) {
        return tiles[tile.r][tile.c];
    };

    RowColPair core_location = RowColPair{
        core_row,
        core_col
    };

    std::unordered_map<RowColPair, int> visited;
    std::vector<std::pair<RowColPair, int>> to_visit;
    to_visit.push_back({core_location, 0});

    while(!to_visit.empty()) {
        const std::pair<RowColPair, int> current_p = to_visit.back();
        const RowColPair current = current_p.first;
        const int current_depth = current_p.second;
        to_visit.pop_back();

        visited.insert({current, current_depth});

        // Test 4 connected tiles
        const RowColPair above = RowColPair{
            current.r - 1,
            current.c
        };
        if (visited.contains(above) &&
            (visited.at(above) != current_depth + 1 && visited.at(above) != current_depth - 1)) {
            M_Assert(false, "Graph cannot contain a cycle");
        }
        if (is_in_bounds(above) && get_tile_type(above) == TileType::Path && !visited.contains(above)) {
            to_visit.push_back({above, current_depth + 1});
        }

        const RowColPair below = RowColPair{
            current.r + 1,
            current.c
        };
        if (visited.contains(below) &&
            (visited.at(below) != current_depth + 1 && visited.at(below) != current_depth - 1)) {
            M_Assert(false, "Graph cannot contain a cycle");
        }
        if (is_in_bounds(below) && get_tile_type(below) == TileType::Path && !visited.contains(below)) {
            to_visit.push_back({below, current_depth + 1});
        }

        const RowColPair right = RowColPair{
            current.r,
            current.c + 1
        };
        if (visited.contains(right) &&
            (visited.at(right) != current_depth + 1 && visited.at(right) != current_depth - 1)) {
            M_Assert(false, "Graph cannot contain a cycle");
        }
        if (is_in_bounds(right) && get_tile_type(right) == TileType::Path && !visited.contains(right)) {
            to_visit.push_back({right, current_depth + 1});
        }

        const RowColPair left = RowColPair{
            current.r,
            current.c - 1
        };
        if (visited.contains(left) &&
            (visited.at(left) != current_depth + 1 && visited.at(left) != current_depth - 1)) {
            M_Assert(false, "Graph cannot contain a cycle");
        }
        if (is_in_bounds(left) && get_tile_type(left) == TileType::Path && !visited.contains(left)) {
            to_visit.push_back({left, current_depth + 1});
        }

        if (is_on_edge(current)) {
            if (is_on_edge(above) && get_tile_type(above) == TileType::Path ||
                is_on_edge(below) && get_tile_type(below) == TileType::Path ||
                is_on_edge(right) && get_tile_type(right) == TileType::Path ||
                is_on_edge(left)  && get_tile_type(left)  == TileType::Path) {
                M_Assert(false, "Entry-points (tiles on the edge) can only be 1-wide.");
            }
            entry_points.push_back({current.r, current.c});
        }

        const auto current_is_dead_end = [&]() {
            return !is_on_edge(current) &&
                (get_tile_type(above) != TileType::Path || visited.contains(above)) &&
                (get_tile_type(below) != TileType::Path || visited.contains(below)) &&
                (get_tile_type(right) != TileType::Path || visited.contains(right)) &&
                (get_tile_type(left)  != TileType::Path || visited.contains(left));
        };

        if (current_is_dead_end()) {
            M_Assert(false, "Map has a path with a dead-end (path that does not lead to the edge of the map).");
        }
    }

    // Make sure that all valid paths are connected to the core
    for (int r = 0; r < num_rows; ++r) {
        for (int c = 0; c < num_cols; ++c) {
            const TileType ty = tiles[r][c];
            if (ty == TileType::Path && !visited.contains(RowColPair{r, c})) {
                M_Assert(false, "All paths must be connected to the core.");
            }
        }
    }

    return MapLayout{
        std::move(tiles),
        std::move(entry_points)
    };
}

void MapLayout::print() const {
    for (const std::vector<TileType>& ts : tiles) {
        for (const TileType& t : ts) {
            std::print("{}", tile_type_to_char(t));
        }
        std::print("\n");
    }
}


Map::Map(VkEngine* engine, MapLayout& layout) {
    constexpr float cube_scale = 10.0;

    for (int r = 0; r < layout.tiles.size(); ++r) {
        std::vector<std::unique_ptr<Cube>> cube_line;
        for (int c = 0; c < layout.tiles[0].size(); ++c) {
            const TileType tile = layout.tiles[r][c];

            const glm::vec3 translate = glm::vec3(
                c * cube_scale,
                tile == TileType::Wall ? cube_scale : 0.0,
                r * cube_scale
            );
            const glm::quat rotate = glm::quat();
            const glm::vec3 scale = glm::vec3(10.0);
            const glm::vec4 color = tile_type_to_color(tile);

            cube_line.emplace_back(std::make_unique<Cube>(engine, "cube", translate, rotate, scale, color));

            if (tile == TileType::Core) {
                const glm::vec4 core_model_color = glm::vec4(252 / 255., 65 / 255., 18 / 255., 1.0);
                const glm::vec3 core_model_scale = glm::vec3(5.0);
                const glm::vec3 core_model_translate = glm::vec3(
                    translate.x,
                    cube_scale,
                    translate.z
                );
                core_model = std::make_unique<Cube>(engine, "core", core_model_translate, rotate, core_model_scale, core_model_color);
            }
        }
        map_cubes.push_back(std::move(cube_line));
    }

    for (const std::pair<int, int> coord : layout.entry_points) {
        int r = coord.first;
        int c = coord.second;

        const int spawn_area_scale = 2;
        if (r == 0) {
            r -= spawn_area_scale;
        } else if (c == 0) {
            c -= spawn_area_scale;
        } else if (r == layout.tiles.size() - 1) {
            r += spawn_area_scale;
        } else if (c == layout.tiles[0].size() - 1) {
            c += spawn_area_scale;
        }

        const glm::vec3 translate = glm::vec3(
            c * cube_scale,
            0.0,
            r * cube_scale
        );
        const glm::quat rotate = glm::quat();
        const float xz_scale = 10.0 * (spawn_area_scale + 1);
        const glm::vec3 scale = glm::vec3(xz_scale, 10.0, xz_scale);
        const glm::vec4 color = tile_type_to_color(TileType::Path);

        spawn_cubes.emplace_back(std::make_unique<Cube>(engine, "spawn cube", translate, rotate, scale, color));
    }
}

void Map::draw(const glm::mat4& top_matrix, DrawContext& ctx) const {
    for (const auto& line : map_cubes) {
        for (const auto& cube : line) {
            cube->draw(top_matrix, ctx);
        }
    }

    for (const auto& cube : spawn_cubes) {
        cube->draw(top_matrix, ctx);
    }

    core_model->draw(top_matrix, ctx);
}