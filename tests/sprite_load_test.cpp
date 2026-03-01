#include <gtest/gtest.h>
#include "../include/enjin2/graphics/sprite_asset.hpp"
#include "../include/enjin2/scripting/lua_wrapper.hpp"

#include <fstream>
#include <vector>

using namespace enjin2;

// Load a file into a buffer
static std::vector<uint8_t> loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

// -----------------------------------------------------------------------------
// Binary Format Tests
// -----------------------------------------------------------------------------

TEST(NjnFormatTest, HeaderSizeIsExactly8) {
    EXPECT_EQ(sizeof(NjnHeader), 8);
}

TEST(NjnFormatTest, ParseValidHeader) {
    uint8_t dummy[8 + 10*10] = {0};
    dummy[0] = 'N'; dummy[1] = 'J';
    dummy[2] = 1; // version
    dummy[3] = 10; // cellW
    dummy[4] = 10; // cellH
    dummy[5] = 1;  // cols
    dummy[6] = 1;  // rows

    NjnHeader h;
    EXPECT_TRUE(parseNjnHeader(dummy, sizeof(dummy), h));
    EXPECT_EQ(h.cellW, 10);
    EXPECT_EQ(h.cellH, 10);
    EXPECT_EQ(h.cols, 1);
    EXPECT_EQ(h.rows, 1);
    EXPECT_EQ(njnPixelDataSize(h), 100);
}

TEST(NjnFormatTest, RejectInvalidMagic) {
    uint8_t dummy[8 + 10*10] = {0};
    dummy[0] = 'X'; dummy[1] = 'X';
    dummy[2] = 1; dummy[3] = 10; dummy[4] = 10; dummy[5] = 1; dummy[6] = 1;

    NjnHeader h;
    EXPECT_FALSE(parseNjnHeader(dummy, sizeof(dummy), h));
}

TEST(NjnFormatTest, RejectTruncatedData) {
    uint8_t dummy[8 + 50] = {0}; // Missing 50 bytes of pixel data
    dummy[0] = 'N'; dummy[1] = 'J';
    dummy[2] = 1; dummy[3] = 10; dummy[4] = 10; dummy[5] = 1; dummy[6] = 1;

    NjnHeader h;
    EXPECT_FALSE(parseNjnHeader(dummy, sizeof(dummy), h)); // should fail because size < 8 + 100
}

TEST(NjnFormatTest, RejectZeroDimensions) {
    uint8_t dummy[8] = {0};
    dummy[0] = 'N'; dummy[1] = 'J'; dummy[2] = 1;

    NjnHeader h;
    EXPECT_FALSE(parseNjnHeader(dummy, sizeof(dummy), h));
}

// -----------------------------------------------------------------------------
// Lua Bindings Tests
// -----------------------------------------------------------------------------

class SpriteAssetLoaderTest : public ::testing::Test {
protected:
    LuaWrapper lua_;

    void SetUp() override {
        ASSERT_TRUE(lua_.initialize());
        lua_.getBindings().setAssetPath("tests"); // Load from tests dir where CMake copies
    }

    void TearDown() override {
        lua_.shutdown();
    }
};

TEST_F(SpriteAssetLoaderTest, LoadValidSpriteReturnsHandle) {
    // Lua script to load pikachu.njn
    const char* script = R"(
        test_handle = engine.sprite.load("test_pikachu")
        return test_handle
    )";

    LuaResult res = lua_.execute(script);
    EXPECT_TRUE(res.success);

    // Verify it returns a valid handle (0)
    lua_State* L = lua_.getEngine().getState();
    lua_getglobal(L, "test_handle");
    EXPECT_TRUE(lua_isinteger(L, -1));
    int handle = lua_tointeger(L, -1);
    lua_pop(L, 1);

    EXPECT_EQ(handle, 0); // Should get first slot
}

TEST_F(SpriteAssetLoaderTest, LoadMissingSpriteReturnsNegOne) {
    const char* script = R"(
        test_handle = engine.sprite.load("does_not_exist")
    )";

    LuaResult res = lua_.execute(script);
    EXPECT_TRUE(res.success);

    lua_State* L = lua_.getEngine().getState();
    lua_getglobal(L, "test_handle");
    EXPECT_TRUE(lua_isinteger(L, -1));
    int handle = lua_tointeger(L, -1);
    lua_pop(L, 1);

    EXPECT_EQ(handle, -1);
}

TEST_F(SpriteAssetLoaderTest, PoolFullReturnsNegOne) {
    // Fill all 16 slots
    for (int i = 0; i < 16; ++i) {
        LuaResult res = lua_.execute("return engine.sprite.load('test_pikachu')");
        EXPECT_TRUE(res.success);
    }

    // 17th should fail
    LuaResult res = lua_.execute("test_handle = engine.sprite.load('test_pikachu')");
    EXPECT_TRUE(res.success);

    lua_State* L = lua_.getEngine().getState();
    lua_getglobal(L, "test_handle");
    EXPECT_TRUE(lua_isinteger(L, -1));
    int handle = lua_tointeger(L, -1);
    lua_pop(L, 1);

    EXPECT_EQ(handle, -1);
}

TEST_F(SpriteAssetLoaderTest, DrawLoadedSprite) {
    // Provide a 4-bit canvas to draw onto
    Canvas4<64, 64> canvas;
    LuaCanvas lua_canvas(&canvas);
    lua_.setCanvas(&lua_canvas);

    // Load and draw pikachu
    const char* script = R"(
        handle = engine.sprite.load("test_pikachu")
        if handle >= 0 then
            drawSprite(handle, 10, 10)
        end
    )";

    LuaResult res = lua_.execute(script);
    EXPECT_TRUE(res.success);

    // Verify some non-transparent pixels were drawn. Pikachu is approx 38x38.
    // Transparent index is 15. The canvas clears to 0 by default.
    bool found_drawn_pixel = false;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (canvas.getPixel(x, y).value != 0 && canvas.getPixel(x, y).value != 15) {
                found_drawn_pixel = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found_drawn_pixel);
}

TEST_F(SpriteAssetLoaderTest, FreeAndReloadSprite) {
    // Load first slot
    lua_.execute("h1 = engine.sprite.load('test_pikachu')");
    
    // Free it
    lua_.execute("freeSprite(h1)");

    // Load again, should reuse slot 0
    lua_.execute("h2 = engine.sprite.load('test_pikachu')");

    lua_State* L = lua_.getEngine().getState();
    lua_getglobal(L, "h2");
    int h2 = lua_tointeger(L, -1);
    lua_pop(L, 1);

    EXPECT_EQ(h2, 0); // Slot 0 was reclaimed
}
