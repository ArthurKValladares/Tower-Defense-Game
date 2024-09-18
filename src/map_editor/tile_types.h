// TODO: Instructions for how to add new tiles later

/*
#define TILE_TYPE_TO_COLOR_CASE_I(TileType, Char, R, G, B, ...)\
case TileType: {\
    return glm::vec4(R / 255., G / 255., B / 255., 1.0);\
}
#define TILE_TYPE_TO_CHAR_CASE(...) TILE_TYPE_TO_COLOR_CASE_I(__VA_ARGS__)

#define CHAR_TO_TILE_TYPE_CASE_I(TileType, Char, ...)\
case Char: {\
    return TileType;\
}
#define CHAR_TO_TILE_TYPE_CASE(...) CHAR_TO_TILE_TYPE_CASE_I(__VA_ARGS__)
*/
enum class TileType : unsigned char {
    Invalid = 0,
    Path,
    Wall,
    Core,
};

// TODO: I wish this looked better and was easier to work with the other macros, think about it later
#define PATH_TILE TileType::Path, ' ', 60, 168, 50
#define WALL_TILE TileType::Wall, '#', 64, 64, 64
#define CORE_TILE TileType::Core, 'O', 199, 40, 0

//
// TileType -> char function
//

#define TILE_TYPE_TO_CHAR_CASE(TileType, Char, ...) \
case TileType: { \
    return Char; \
}

static char tile_type_to_char_test(TileType ty) {
    switch (ty) {
        TILE_TYPE_TO_CHAR_CASE(PATH_TILE)
        case TileType::Invalid:
        default: {
            M_Assert(false, "Tile type not supported");
            return '\0';
        }
    }
}

#undef TILE_TYPE_TO_CHAR_CASE_I
#undef TILE_TYPE_TO_CHAR_CASE

#undef PATH_TILE
#undef WALL_TILE
#undef CORE_TILE