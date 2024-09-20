enum class TileType : unsigned char {
    Invalid = 0,
    Path,
    Wall,
    Core,
};

static char tile_type_to_char_test(TileType ty) {
    switch (ty) {
/*[[[cog
import cog
cases = [['Path', ' '], ['Wall', '#'], ['Core', 'O']]
for case in cases:
    tile_type = case[0]
    tile_char = case[1]
    cog.outl("      case TileType::%s: { return '%s'; }" % (tile_type, tile_char))
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