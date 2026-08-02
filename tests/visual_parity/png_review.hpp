#ifndef ENJIN2_TESTS_VISUAL_PARITY_PNG_REVIEW_HPP
#define ENJIN2_TESTS_VISUAL_PARITY_PNG_REVIEW_HPP

// Visual Parity Bench — human review path (design § 9).
//
// For each failure signature that has repro planes, writes one triptych PNG:
// BASE | HEAD | diff, x4 nearest-neighbour, diff pixels in magenta over the
// dimmed BASE image. Written only when the bench is run with --png DIR —
// explicit, never automatic; advisory, never a gate. Delivery to the tailnet
// phone stays a human command (the bench prints the `tailscale file cp`
// line, it does not run it).

// STATIC keeps the stb implementation internal to this TU:
// src/graphics/canvas.cpp already emits the extern "C" stbi_write_* symbols
// inside enjin2_graphics, and a second external definition here would collide
// the moment the linker pulls both objects.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "bench_support.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace parity
{

    inline std::string sanitizeForFilename(const std::string &s)
    {
        std::string out;
        for (char c : s)
            out += (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')
                       ? c
                       : '_';
        return out;
    }

    // 0-15 plane value -> display gray (x17 spreads the nibble to 0-255).
    inline uint8_t displayGray(uint8_t v) { return static_cast<uint8_t>(v * 17); }

    constexpr int kScale = 4;
    constexpr int kGutter = 2;

    inline bool writeTriptychPng(const std::string &path,
                                 const uint8_t *planeA, const uint8_t *planeC)
    {
        const int panelW = W * kScale;
        const int panelH = H * kScale;
        const int imgW = panelW * 3 + kGutter * 2;
        const int imgH = panelH;
        std::vector<uint8_t> rgb(static_cast<size_t>(imgW) * imgH * 3, 40);

        auto put = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b)
        {
            size_t i = (static_cast<size_t>(y) * imgW + x) * 3;
            rgb[i] = r;
            rgb[i + 1] = g;
            rgb[i + 2] = b;
        };

        for (int py = 0; py < H; ++py)
        {
            for (int px = 0; px < W; ++px)
            {
                const size_t idx = static_cast<size_t>(py) * W + px;
                const uint8_t a = displayGray(planeA[idx]);
                const uint8_t c = displayGray(planeC[idx]);
                const bool differs = planeA[idx] != planeC[idx];
                // diff panel: magenta where the planes differ, dimmed BASE
                // elsewhere so the divergence sits in visual context.
                const uint8_t dr = differs ? 255 : static_cast<uint8_t>(a / 3);
                const uint8_t dg = differs ? 0 : static_cast<uint8_t>(a / 3);
                const uint8_t db = differs ? 255 : static_cast<uint8_t>(a / 3);
                for (int sy = 0; sy < kScale; ++sy)
                {
                    for (int sx = 0; sx < kScale; ++sx)
                    {
                        const int yy = py * kScale + sy;
                        const int xx = px * kScale + sx;
                        put(xx, yy, a, a, a);
                        put(panelW + kGutter + xx, yy, c, c, c);
                        put(2 * (panelW + kGutter) + xx, yy, dr, dg, db);
                    }
                }
            }
        }
        return stbi_write_png(path.c_str(), imgW, imgH, 3, rgb.data(), imgW * 3) != 0;
    }

    // Writes one PNG per signature that carries planes; returns the file list.
    inline std::vector<std::string> writeReviewPngs(const Bench &bench,
                                                    const std::string &dir)
    {
        std::vector<std::string> written;
        int i = 0;
        for (const auto &s : bench.signatures)
        {
            ++i;
            if (s.planeA.empty())
                continue; // metric signature — nothing to render
            char name[160];
            snprintf(name, sizeof(name), "%02d_%s.png", i,
                     sanitizeForFilename(s.pairId).c_str());
            std::string path = dir + "/" + name;
            if (writeTriptychPng(path, s.planeA.data(), s.planeC.data()))
                written.push_back(path);
            else
                printf("  ! failed to write %s\n", path.c_str());
        }
        return written;
    }

} // namespace parity

#endif // ENJIN2_TESTS_VISUAL_PARITY_PNG_REVIEW_HPP
