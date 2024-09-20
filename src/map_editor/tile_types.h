/*[[[cog
import cog
cases = [
    ['Path', ' ', 60, 168, 50],
    ['Wall', '#', 64, 64, 64],
    ['Core', 'O', 199, 40, 0]]
]]]*/
//[[[end]]]

enum class TileType : unsigned char {
    Invalid = 0,
    Path,
    Wall,
    Core,
};

static char tile_type_to_char(TileType ty) {
    switch (ty) {
/*[[[cog
for case in cases:
    tile_type = case[0]
    tile_char = case[1]
    cog.outl("case TileType::%s: { return '%s'; }" % (tile_type, tile_char))
]]]*/
case TileType::Path: { return ' '; }
case TileType::Wall: { return '#'; }
case TileType::Core: { return 'O'; }
//[[[end]]]
        default: {
            M_Assert(false, "Tile type not supported");
            return '\0';
        }
    }
}

static TileType char_to_tile_type(char c) {
    switch (c) {
/*[[[cog
for case in cases:
    tile_type = case[0]
    tile_char = case[1]
    cog.outl("case '%s': { return TileType::%s; }" % (tile_char, tile_type))
]]]*/
case ' ': { return TileType::Path; }
case '#': { return TileType::Wall; }
case 'O': { return TileType::Core; }
//[[[end]]]
        default: {
            M_Assert(false, "Char does not match a TileType");
            return TileType::Invalid;
        }
    }
}

static glm::vec4 tile_type_to_color(TileType ty) {
    switch (ty) {
/*[[[cog
for case in cases:
    tile_type = case[0]
    r = case[2]
    g = case[3]
    b = case[4]
    cog.outl("case TileType::%s: { return glm::vec4(%d / 255., %d / 255., %d / 255., 1.0); }" % (tile_type, r, g, b))
]]]*/
case TileType::Path: { return glm::vec4(60 / 255., 168 / 255., 50 / 255., 1.0); }
case TileType::Wall: { return glm::vec4(64 / 255., 64 / 255., 64 / 255., 1.0); }
case TileType::Core: { return glm::vec4(199 / 255., 40 / 255., 0 / 255., 1.0); }
//[[[end]]]
        default: {
            M_Assert(false, "Tile type not supported");
            return glm::vec4(1.0, 0.0, 1.0, 1.0);
        }
    }
}