// Palette files (Tomodachi ADR-0019, #423): a Lua chunk that returns a table of
// name, author, source and the 8 Palette roles as "#rrggbb". The same format
// for the built-in presets and for palette files on the card.
#include <enjin2/graphics/palette_file.hpp>

#include <cstdio>
#include <cstring>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

static const char* kNyx8Light =
    "return {\n"
    "  name = \"Nyx8 Light\",\n"
    "  author = \"Javier Guerrero\",\n"
    "  source = \"https://lospec.com/palette-list/nyx8\",\n"
    "  bg = \"#f6d6bd\", surface = \"#c3a38a\", muted = \"#997577\", text = \"#08141e\",\n"
    "  accent = \"#0f2a3f\", accent2 = \"#20394f\", accent3 = \"#4e495f\", accent4 = \"#816271\",\n"
    "}\n";

static void test_reads_a_palette_file()
{
    printf("--- a palette file reads into name, credits and 8 roles ---\n");
    PaletteFile pal;
    std::string error;
    ASSERT(parsePaletteFile(kNyx8Light, pal, &error), "Nyx8 Light parses");
    ASSERT(error.empty(), "no error");
    ASSERT(pal.name == "Nyx8 Light", "name");
    ASSERT(pal.author == "Javier Guerrero", "author");
    ASSERT(pal.source == "https://lospec.com/palette-list/nyx8", "source");
    ASSERT(pal.colors[ROLE_BG] == RGB(0xf6, 0xd6, 0xbd), "bg");
    ASSERT(pal.colors[ROLE_SURFACE] == RGB(0xc3, 0xa3, 0x8a), "surface");
    ASSERT(pal.colors[ROLE_MUTED] == RGB(0x99, 0x75, 0x77), "muted");
    ASSERT(pal.colors[ROLE_TEXT] == RGB(0x08, 0x14, 0x1e), "text");
    ASSERT(pal.colors[ROLE_ACCENT] == RGB(0x0f, 0x2a, 0x3f), "accent");
    ASSERT(pal.colors[ROLE_ACCENT2] == RGB(0x20, 0x39, 0x4f), "accent2");
    ASSERT(pal.colors[ROLE_ACCENT3] == RGB(0x4e, 0x49, 0x5f), "accent3");
    ASSERT(pal.colors[ROLE_ACCENT4] == RGB(0x81, 0x62, 0x71), "accent4");
}

// The Nyx8 Light file with one line swapped for another.
static std::string withLine(const char* from, const char* to)
{
    std::string s = kNyx8Light;
    const size_t at = s.find(from);
    if (at != std::string::npos) s.replace(at, std::strlen(from), to);
    return s;
}

static bool rejects(const std::string& source, const char* reasonPart)
{
    PaletteFile pal;
    std::string error;
    const bool ok = parsePaletteFile(source, pal, &error);
    if (!ok && error.find(reasonPart) == std::string::npos)
        fprintf(stderr, "  error was: %s\n", error.c_str());
    return !ok && error.find(reasonPart) != std::string::npos;
}

static void test_rejects_bad_files()
{
    printf("--- a bad palette file fails with a reason ---\n");
    ASSERT(rejects(withLine("accent3 = \"#4e495f\", ", ""), "accent3 is missing"),
           "a missing role is named");
    ASSERT(rejects(withLine("\"#997577\"", "\"#99757\""), "muted is not #rrggbb"),
           "a short hex is rejected");
    ASSERT(rejects(withLine("\"#997577\"", "\"#9975777\""), "muted"), "a long hex is rejected");
    ASSERT(rejects(withLine("\"#997577\"", "\"997577\""), "muted"), "a hex without # is rejected");
    ASSERT(rejects(withLine("\"#997577\"", "\"#99757g\""), "muted"), "a non-hex digit is rejected");
    ASSERT(rejects(withLine("\"#997577\"", "0x997577"), "muted"), "a number is rejected");
    ASSERT(rejects(withLine("name = \"Nyx8 Light\",", ""), "name is missing"), "no name");
    ASSERT(rejects(withLine("\"Nyx8 Light\"", "\"\""), "name is missing"), "an empty name");
    ASSERT(rejects(withLine("\"Javier Guerrero\"", "42"), "author"), "a non-string author");
    ASSERT(rejects("return 42", "must return a table"), "not a table");
    ASSERT(rejects("return {", "palette file:1:"), "a syntax error gives its line");
    ASSERT(rejects("while true do end", "instruction limit"), "a runaway loop is stopped");
    ASSERT(rejects("return { name = os.getenv('HOME') }", ""), "no libraries in the file's environment");
}

static void test_loads_from_disk()
{
    printf("--- loadPaletteFile reads a file of at most 4 KB ---\n");
    const char* path = "palette_file_test.lua";
    FILE* f = fopen(path, "wb");
    fputs(kNyx8Light, f);
    fclose(f);
    PaletteFile pal;
    std::string error;
    ASSERT(loadPaletteFile(path, pal, &error) && pal.name == "Nyx8 Light", "reads the file");

    f = fopen(path, "wb");
    fputs(kNyx8Light, f);
    for (int i = 0; i < 5000; ++i) fputc(' ', f);
    fclose(f);
    ASSERT(!loadPaletteFile(path, pal, &error) && error.find("4 KB") != std::string::npos,
           "a file over 4 KB is refused");
    remove(path);
    ASSERT(!loadPaletteFile(path, pal, &error), "a missing file fails");
}

int main()
{
    test_reads_a_palette_file();
    test_rejects_bad_files();
    test_loads_from_disk();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
