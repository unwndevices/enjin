window.BENCHMARK_DATA = {
  "lastUpdate": 1790796581378,
  "repoUrl": "https://github.com/unwndevices/enjin",
  "entries": {
    "enjin2 Benchmarks": [
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "8b5ef356d5a3efb79935f29d95f7d02b4dde219e",
          "message": "fix(64-02): add __gc metamethod to ObjectProxy to prevent heap-use-after-free\n\nWhen Lua GC collected an ObjectProxy userdata, no __gc metamethod existed to\nclear the Object::m_luaProxy back-pointer. Consequently Object::~Object()\nwould write `proxy->valid = false` into already-freed Lua heap memory (ASAN:\nheap-use-after-free in object.cpp:12).\n\nFix: register a __gc metamethod on the ObjectProxy metatable that calls\n`obj->setLuaProxy(nullptr)` before Lua frees the proxy. Object::~Object()\nnow finds m_luaProxy == nullptr and skips the write safely.\n\nVerified: bench_lua runs clean under AddressSanitizer with detect_leaks=0.\nTriggered by CI run 22818751543 failing with malloc heap corruption during\nlua GC: full collect benchmark.\n\nCo-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>",
          "timestamp": "2026-03-08T11:07:01+01:00",
          "tree_id": "bfc6e29be3111408201bb4442ac4c0c11fcc4ddd",
          "url": "https://github.com/unwndevices/enjin/commit/8b5ef356d5a3efb79935f29d95f7d02b4dde219e"
        },
        "date": 1772964469610,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 50,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 220,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 71693,
            "range": "± 0.01%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4298,
            "range": "± 0.23%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 300,
            "range": "± 0.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 782,
            "range": "± 0.13%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1433,
            "range": "± 0.84%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2734.5,
            "range": "± 1.09%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3947.5,
            "range": "± 0.75%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 80,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 3.23%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 310.5,
            "range": "± 0.16%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 60252.5,
            "range": "± 3.21%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 972,
            "range": "± 3.57%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1793.5,
            "range": "± 4.47%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2620,
            "range": "± 2.34%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2455,
            "range": "± 2.94%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1548,
            "range": "± 4.38%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3787,
            "range": "± 0.65%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices",
            "email": "ciro@unwn.dev"
          },
          "committer": {
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices",
            "email": "ciro@unwn.dev"
          },
          "id": "8b5ef356d5a3efb79935f29d95f7d02b4dde219e",
          "message": "fix(64-02): add __gc metamethod to ObjectProxy to prevent heap-use-after-free\n\nWhen Lua GC collected an ObjectProxy userdata, no __gc metamethod existed to\nclear the Object::m_luaProxy back-pointer. Consequently Object::~Object()\nwould write `proxy->valid = false` into already-freed Lua heap memory (ASAN:\nheap-use-after-free in object.cpp:12).\n\nFix: register a __gc metamethod on the ObjectProxy metatable that calls\n`obj->setLuaProxy(nullptr)` before Lua frees the proxy. Object::~Object()\nnow finds m_luaProxy == nullptr and skips the write safely.\n\nVerified: bench_lua runs clean under AddressSanitizer with detect_leaks=0.\nTriggered by CI run 22818751543 failing with malloc heap corruption during\nlua GC: full collect benchmark.\n\nCo-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>",
          "timestamp": "2026-03-08T10:07:01Z",
          "url": "https://github.com/unwndevices/enjin/commit/8b5ef356d5a3efb79935f29d95f7d02b4dde219e"
        },
        "date": 1772964533029,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 121,
            "range": "± 3.88%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 220,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 71715,
            "range": "± 0.03%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4789,
            "range": "± 0.42%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 300,
            "range": "± 3.09%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 801,
            "range": "± 1.14%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1463,
            "range": "± 0.72%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2735,
            "range": "± 0.73%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3967,
            "range": "± 0.38%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 10.89%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 1.61%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0.83%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 65202,
            "range": "± 4.42%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 951.5,
            "range": "± 4.45%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1728.5,
            "range": "± 2.3%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2710,
            "range": "± 1.49%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2549.5,
            "range": "± 3.06%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1508.5,
            "range": "± 3.97%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3683.2027,
            "range": "± 0.78%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "a7f7284cc6ea7d87c511a5c3594842f9d4049e0e",
          "message": "docs(phase-65): complete phase execution",
          "timestamp": "2026-03-08T13:15:37+01:00",
          "tree_id": "4533b86de612f71c70d6bb405cce3f425d06b7c6",
          "url": "https://github.com/unwndevices/enjin/commit/a7f7284cc6ea7d87c511a5c3594842f9d4049e0e"
        },
        "date": 1772976057591,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 220,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 71673,
            "range": "± 0.01%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4278,
            "range": "± 0.23%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 291,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 792,
            "range": "± 1.25%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1453,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2823.1865,
            "range": "± 0.99%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 4057,
            "range": "± 0.72%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 10.0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 57362,
            "range": "± 3.45%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 1007,
            "range": "± 2.65%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1743.5,
            "range": "± 3.47%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2705.5,
            "range": "± 2.04%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2564.5,
            "range": "± 2.29%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1503,
            "range": "± 4.97%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3776.5,
            "range": "± 0.53%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "55bd794d93585ecb1711688202714588bf180239",
          "message": "refactor: migrate bare graphics globals to gfx.* namespace table\n\n- Replace engine->registerFunction() calls with lua_newtable + lua_setfield pattern\n- Nest LAYER_BG/MID/FG/UI/DEBUG and COLOR under gfx.*\n- Remove love.graphics prototype (dead code)\n- Remove input polling globals (isButtonHeld, isButtonJustPressed, etc.)\n- Keep BTN and print() as bare globals\n- Update all test inline Lua strings (~50 occurrences across 8 files)\n- Update all demo scripts (~160 occurrences across 10 files)\n- Add test_bare_globals_removed() verifying clean break (API-04)\n\nCo-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>",
          "timestamp": "2026-03-16T16:04:53+01:00",
          "tree_id": "8ff2000c95442862265e285e159f32a37432241d",
          "url": "https://github.com/unwndevices/enjin/commit/55bd794d93585ecb1711688202714588bf180239"
        },
        "date": 1773673694462,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 19,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 62,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 35,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 214.5,
            "range": "± 0.7%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 69180,
            "range": "± 3.37%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 18,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 692,
            "range": "± 0.14%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4485.5,
            "range": "± 0.08%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 242.5,
            "range": "± 1.89%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 689.5,
            "range": "± 2.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1307.5,
            "range": "± 1.61%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2377,
            "range": "± 1.76%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3544,
            "range": "± 1.32%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 77,
            "range": "± 1.28%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 85,
            "range": "± 1.16%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 23,
            "range": "± 4.17%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 62,
            "range": "± 1.59%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 106,
            "range": "± 0.94%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 191,
            "range": "± 0.53%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 277.5,
            "range": "± 0.18%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 50128,
            "range": "± 3.18%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 942,
            "range": "± 2.12%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1767,
            "range": "± 4.0%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2731.5,
            "range": "± 1.96%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2486.5,
            "range": "± 5.87%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1509.5,
            "range": "± 2.74%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 2889.7669,
            "range": "± 1.0%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "918e2bb9488785d1fb6ffa74211463dc128a1735",
          "message": "feat: upstream Eisei step-0 delta batch (getTextWidth, SCREEN_CENTER, warning hygiene, srcFilter)\n\nPorts the vendored-only engine deltas from unwndevices/unwn into upstream\nenjin, as the step-0 batch for the Eisei→enjin migration (unwn#118, ADR-0003).\n\n- GFX-font-aware getTextWidth(): sum glyph xAdvance and trim the trailing\n  space on the last glyph so custom-font text measures its visual extent\n  instead of the post-cursor position (fixes C_List mis-centering).\n- SCREEN_CENTER_X/Y constants (new include/enjin2/screen.hpp); polar.hpp\n  default arg and C_Drawable::abs_center now reference them.\n- Warning hygiene: rename PackedPixel4 ctor param byte->raw (-Wshadow) and\n  cast uint16_t loop indices to int16_t in the texture add/subtract blends\n  (-Wsign-conversion) so consuming ESP32 TUs stop re-spraying warnings.\n- library.json srcFilter: switch to an allowlist (-<*> +<effects/postfx.cpp>)\n  so consumers no longer pull the SDL/headless duplicate-main() hazard;\n  PlatformIO has no consumer-side per-dep override, so the fix lands here once.\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>",
          "timestamp": "2026-07-12T18:56:28+02:00",
          "tree_id": "847bf3df2da2c1d6f9af3e9ef6650f6a9193b817",
          "url": "https://github.com/unwndevices/enjin/commit/918e2bb9488785d1fb6ffa74211463dc128a1735"
        },
        "date": 1783875456549,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 220.5,
            "range": "± 0.23%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 71793.5,
            "range": "± 0.04%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 993.0114,
            "range": "± 0.4%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4408,
            "range": "± 0.23%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 291,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 796.5,
            "range": "± 0.69%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1463,
            "range": "± 0.69%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2765,
            "range": "± 0.73%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3988,
            "range": "± 0.28%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 1.1%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 66003,
            "range": "± 3.1%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 972,
            "range": "± 2.06%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1698,
            "range": "± 2.71%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2620,
            "range": "± 1.95%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2550,
            "range": "± 3.21%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1517.5,
            "range": "± 2.97%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3677,
            "range": "± 0.57%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "58692249+unwndevices@users.noreply.github.com",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "05918d5ce0b98a2c2387d3d58e95915364585bc1",
          "message": "fix(docs): resolve all 28 Doxygen warnings blocking the docs CI gate (#1)\n\nThe Deploy Documentation workflow has failed on every push since early\nMarch because the Doxygen warning count (28) exceeded the CI threshold\n(20). Document all undocumented members/params/returns flagged by the\nwarning log, and escape a '#ifdef' reference that Doxygen parsed as an\nexplicit link request.\n\nAlso cat the full warning log in the CI step output on failure (before\nthis, the warnings were only in a file on the runner and the first 30\nlines of the step summary, making the failure hard to diagnose), and\nrefresh the generated docs/api pages that had gone stale while the\ngate was failing.\n\nVerified locally with doxygen 1.16.1: 0 warnings, generate-api-docs.js\nproduces 81 pages, docusaurus build succeeds.\n\nCo-authored-by: Claude Fable 5 <noreply@anthropic.com>",
          "timestamp": "2026-07-12T19:16:42+02:00",
          "tree_id": "090f7dd32bebc34e753b87d70a80c87f649c6a47",
          "url": "https://github.com/unwndevices/enjin/commit/05918d5ce0b98a2c2387d3d58e95915364585bc1"
        },
        "date": 1783876647766,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 220,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 71774,
            "range": "± 0.01%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4378,
            "range": "± 0.23%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 291,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 792,
            "range": "± 1.12%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1447.5,
            "range": "± 0.38%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2745,
            "range": "± 0.72%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 4007,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 85.5,
            "range": "± 6.87%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 3.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 57778,
            "range": "± 6.26%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 992,
            "range": "± 5.56%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1703,
            "range": "± 3.31%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2610,
            "range": "± 1.89%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2560,
            "range": "± 3.69%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1488,
            "range": "± 4.78%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3607.1174,
            "range": "± 0.59%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "58692249+unwndevices@users.noreply.github.com",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "182dbd1110eaa874f4174a3a97b9f9f39b817727",
          "message": "feat(ui): make the upstream ui ECS real (#120) (#2)\n\nPhase 2 of the Eisei↔enjin migration (epic #117): the ui ECS shipped as\nscaffolding (#115) — broken storage, unimplemented query, pseudo-code systems,\nan empty theme, and two never-compiled translation units. Make it real.\n\nECS core\n- ComponentStorage: replace the aliasing function-local `static` backing array\n  with real per-instance packed member storage, and fix the sparse map to index\n  entities by id (was `% CAPACITY`, which collided). Now a correct O(1) sparse\n  set with swap-on-remove; distinct storages no longer share memory.\n- EntityManager: template on MAX_ENTITIES (default 4096, header-only) so a World\n  can size the entity-id space to its capacity; drop the out-of-line defs.\n- world.hpp: new lean, fixed-capacity World<CAPACITY, Components...> registry —\n  the connective tissue the systems were missing. Composes EntityManager + one\n  ComponentStorage per type; create/destroy/valid, add/get/has/remove, and a\n  query<First, Rest...>() that yields entities holding the whole set.\n- ComponentQuery::findNext(): implemented as a real filtered scan over an entity\n  span; World::query() drives it.\n\nSystems (were pseudo-code comments)\n- AnimationSystem / InputSystem / RenderSystem now carry real update() bodies,\n  templated on the World type so each feature context composes its own world.\n\ntheme.hpp: replace the empty placeholder with a constexpr Theme (palette +\nmetrics) and a default dark theme.\n\nHygiene\n- Dedupe GFXfont: drop text_renderer.hpp's duplicate enjin2::GFXfont typedefs and\n  use the canonical global gfxfont.h; simplify the now-identity casts in\n  bindings.cpp.\n- Dedupe ICanvas: delete the dead abstract/icanvas.hpp (included nowhere; the\n  live definition is graphics/canvas.hpp).\n\nBuild/test: wire src/ui/{component,system,theme}.cpp into enjin2_ui (they were\ncompiled into nothing) and add tests/ui_ecs_test.cpp (73 assertions: storage,\nentity manager, world, query, all three systems, theme).\n\nCo-authored-by: Claude Fable 5 <noreply@anthropic.com>",
          "timestamp": "2026-07-13T01:48:17+02:00",
          "tree_id": "dd609f2c425c2f02120cda0efce6100adba37781",
          "url": "https://github.com/unwndevices/enjin/commit/182dbd1110eaa874f4174a3a97b9f9f39b817727"
        },
        "date": 1783900143600,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 221,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 71674,
            "range": "± 0.01%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4279,
            "range": "± 0.21%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 300,
            "range": "± 0.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 801,
            "range": "± 1.14%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1463,
            "range": "± 0.69%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2725,
            "range": "± 0.73%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3987.5,
            "range": "± 0.49%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 85.5,
            "range": "± 6.87%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 64456,
            "range": "± 2.67%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 961,
            "range": "± 4.16%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1673.5,
            "range": "± 2.99%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2695,
            "range": "± 2.03%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2559.5,
            "range": "± 2.37%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1518,
            "range": "± 1.99%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3697,
            "range": "± 0.82%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "58692249+unwndevices@users.noreply.github.com",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "83e470c83b1823203646ebeaa8721881163b6ee9",
          "message": "feat(ui): Phase 3a — upstream the live widgets + animators as data-only ECS (#121) (#4)\n\n* feat(ui): upstream List widget + Easing util as data-only ECS (#121)\n\nPhase 3a of the Eisei->enjin migration: begin rewriting the generic live\nwidgets as data-only Component<T> + SystemBase drawing to ICanvas<Pixel4>\nvia TextRenderer<Pixel4>, per ADR-0004 and the migration spec's split rule.\nNot a port of the Canvas8 member API.\n\nThis first pass lands the shared substrate and the flagship widget:\n\n- ui/easing.hpp: normalized easing curves upstreamed from Libs/enjin/utils\n  (namespace enjin2, EasingFunction pointer type). The animation substrate the\n  deferred animators and widget transitions will draw on.\n- ui/widgets/list.hpp: C_List rewritten as a data-only ListComponent\n  (pre-stringified items; the getString<T> projection moves to the scene/host\n  edge) + ListSystem<TWorld,TCanvas>. Presentation-only: no InputState; the\n  host drives selection, the system draws. Themed via theme.hpp.\n- tests: ui_easing_test (curve endpoints/shape/pointer type) and ui_list_test\n  (selection clamping, marquee advance, render path on Canvas4). Both green.\n\nDeferred to later Phase 3a passes: Label, Icon, Gauge, OverlayBg, PopUp, and\nthe keyframe animators. Two highlight-bar fidelity gaps (square vs rounded\nrect; glyph-bearing vertical offset) are noted in-code for Gate-2 visual parity.\n\nSplit rule (#110): Slider, Tooltip, ButtonDial, Dither, Noise and DrawingHelpers\nare dead in shipping scenes and will be dropped, not rewritten.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>\n\n* feat(ui): upstream Label/Icon/Gauge/Overlay/PopUp + animators as data-only ECS (#121)\n\nContinues Phase 3a of the Eisei->enjin migration: the remaining live widgets are\nrewritten as data-only Component<T> + a System<TWorld,TCanvas> drawing to\nICanvas<Pixel4> via TextRenderer/Primitives, themed via theme.hpp, per ADR-0004\nand the migration spec's split rule. Presentation-only throughout — items arrive\npre-formatted and the host drives; the systems only advance time and draw.\n\nShared substrate:\n- graphics/primitives.hpp: drawRoundRect/fillRoundRect (+ draw/fillCircleHelper)\n  on Primitives<TPixel>/ICanvas, the co-design point flagged in list.hpp. list.hpp\n  now fills its selected-row bar with the rounded helper (radius 2, matching\n  C_List) instead of the square stopgap.\n- ui/animator.hpp: C_PositionAnimator / C_ParameterAnimator<T> / C_KeyframeAnimator\n  collapse into one generic AnimatorComponent<T> (keyframe timeline + clock, pure\n  value() seam over easing.hpp) driven by an AnimatorSystem that only ticks time;\n  applying the value stays host-side. lerpValue handles scalars and Vec2.\n\nWidgets:\n- widgets/label.hpp: C_Label -> LabelComponent (+ pure measurer-injected wrapText\n  seam) + LabelSystem; centered multi-line text with an optional rounded bg panel\n  and tail.\n- widgets/icon.hpp: C_Sprite Icon -> IconComponent (borrowed grayscale bitmap,\n  matte-16 transparency, pure sampleAt/isOpaqueAt) + IconSystem blit.\n- widgets/gauge.hpp: C_FillUpGauge -> GaugeComponent (value clamp + fillRegion/\n  levelLineY seams) + GaugeSystem; dithered fill clipped analytically to the rim,\n  dropping the old offscreen mask canvas.\n- widgets/overlay.hpp: OverlayBg's Sub-blend dim -> OverlayComponent (pure dim())\n  + OverlaySystem; the gradient sprite is now an ordinary IconComponent host-side.\n- widgets/popup.hpp: PopUpUI -> PopUpComponent (two lines, primitive-drawn icons,\n  pure auto-hide advance() seam) + PopUpSystem; PositionComponent marks the card\n  center.\n\nTests: primitives_roundrect + ui_animator/label/icon/gauge/overlay_popup, all\npinning the pure seams first, then a Canvas4 render pass. Full ui suite green\n(18/18); the pre-existing scene_render/shadow_mode C_Camera link failures are\nuntouched and excluded. Vertical text/bar alignment stays a by-eye Gate-2 pass\n(getTextBounds carries no glyph bearing), noted in-code.\n\nSplit rule (#110): Slider, Tooltip, ButtonDial, Dither, Noise and DrawingHelpers\nremain dropped, not rewritten.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>\n\n* refactor(ui): drop dead theme_ member from LabelSystem (#121)\n\nLabelSystem stored a Theme member and took it as a ctor param but never\nread it — labels style per-instance from LabelComponent::color/background,\nexactly like IconSystem (which takes only world+canvas). Code review\nflagged the unused member/param as a Middle Man / Refused Bequest.\n\nRemove the member, the ctor param, and the now-unused theme.hpp include.\nNo caller passed a theme (tests construct LabelSystem(&world, &canvas)),\nso the two-arg form is unchanged in practice. ui suite still 9/9 green.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>\n\n---------\n\nCo-authored-by: Claude Opus 4.8 <noreply@anthropic.com>",
          "timestamp": "2026-07-13T01:50:14+02:00",
          "tree_id": "1e99a43f2b0f6849bd79ce819fec5b3dfc71f481",
          "url": "https://github.com/unwndevices/enjin/commit/83e470c83b1823203646ebeaa8721881163b6ee9"
        },
        "date": 1783900246798,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 20,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 60,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 140,
            "range": "± 0.71%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 37651.5,
            "range": "± 0.23%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 20,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 500,
            "range": "± 0.2%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 2954,
            "range": "± 0.03%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 150,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 441,
            "range": "± 0.23%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 821,
            "range": "± 1.2%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 1573,
            "range": "± 0.57%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 2312.3317,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 50,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 50,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 20,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 70,
            "range": "± 1.41%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 120,
            "range": "± 0.83%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 180,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 27291,
            "range": "± 1.86%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 515.5,
            "range": "± 4.99%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1012,
            "range": "± 1.84%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 1522.5,
            "range": "± 2.01%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 1372,
            "range": "± 3.0%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 821,
            "range": "± 2.5%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 2013,
            "range": "± 0.49%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "58692249+unwndevices@users.noreply.github.com",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "dfce20d587815403a3569eda5755f899851f7f1e",
          "message": "Merge pull request #5 from unwndevices/m2/restores\n\nM1 bench + M2 restores: visual-parity bench, Blit move, odd-width Canvas4, circle + text restores (unwn #164/#165)",
          "timestamp": "2026-08-02T17:24:41+02:00",
          "tree_id": "a7fa02d719f1f75a775d7802e2a6c68268cf92a3",
          "url": "https://github.com/unwndevices/enjin/commit/dfce20d587815403a3569eda5755f899851f7f1e"
        },
        "date": 1785684337806,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 30,
            "range": "± 14.11%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 220,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 69706,
            "range": "± 0.01%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 851,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4807,
            "range": "± 0.21%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 280,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 811,
            "range": "± 2.4%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1462,
            "range": "± 1.35%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2804,
            "range": "± 1.41%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 4068.124,
            "range": "± 0.68%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 100,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 80,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 240.5,
            "range": "± 0.21%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 331,
            "range": "± 1.41%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 52249,
            "range": "± 3.26%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 941.5,
            "range": "± 6.27%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1582.5,
            "range": "± 4.28%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2523.5,
            "range": "± 2.23%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2383.5,
            "range": "± 2.96%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1493,
            "range": "± 3.52%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 4438.6995,
            "range": "± 0.83%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "58692249+unwndevices@users.noreply.github.com",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6453c903f2f8b6a83ae222097e4270e737e43732",
          "message": "Merge pull request #7 from unwndevices/m5/adjudication\n\nM5 sweep adjudication: 13 parity fixes, 2 glcd waivers, 2 retirements (unwn #168)",
          "timestamp": "2026-08-02T20:08:02+02:00",
          "tree_id": "9a6ab1ace241c5dfbac47cce1e11c79c38f1608c",
          "url": "https://github.com/unwndevices/enjin/commit/6453c903f2f8b6a83ae222097e4270e737e43732"
        },
        "date": 1785694140907,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 50,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 220,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 66935,
            "range": "± 0.13%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 4388,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 290,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 791,
            "range": "± 1.2%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1442,
            "range": "± 1.06%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2745,
            "range": "± 0.75%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3942.5,
            "range": "± 0.39%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 1.11%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 3.23%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 220,
            "range": "± 2.36%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 80575.5,
            "range": "± 9.1%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 987,
            "range": "± 4.23%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1708,
            "range": "± 1.75%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2615,
            "range": "± 1.73%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2559.5,
            "range": "± 2.31%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1578,
            "range": "± 3.13%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3657,
            "range": "± 0.56%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "58692249+unwndevices@users.noreply.github.com",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "11f0cd71221086b7bcc892a221adf8adc2bd07db",
          "message": "Merge pull request #8 from unwndevices/feat/canvas-480x480-typedefs\n\nfeat(canvas): add 480×480 canvas type aliases",
          "timestamp": "2026-09-04T19:01:09+02:00",
          "tree_id": "7702eb35932a4e8ef2b7fff0153475b3456182c6",
          "url": "https://github.com/unwndevices/enjin/commit/11f0cd71221086b7bcc892a221adf8adc2bd07db"
        },
        "date": 1788541314671,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 25.5,
            "range": "± 19.59%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 170,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 54169,
            "range": "± 0.02%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 3.23%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 661,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 3878.898,
            "range": "± 0.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 220,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 611,
            "range": "± 0.08%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1131,
            "range": "± 0.84%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2143,
            "range": "± 0.94%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3165,
            "range": "± 0.61%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 71,
            "range": "± 1.43%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 60,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 100,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 180,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 260,
            "range": "± 0.38%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 41846.5,
            "range": "± 2.69%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 741,
            "range": "± 4.22%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1247,
            "range": "± 2.51%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2023,
            "range": "± 1.72%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 1863,
            "range": "± 3.76%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1102,
            "range": "± 3.11%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3415,
            "range": "± 0.58%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "2e5eb7f9fbd25dee8dcaba86f56f02b682312922",
          "message": "merge: presenter dirty-tiles + spring + span borders into main\n\nBrings the #34-#39 line (dirty-tile compositor, Remap tint, style slots,\nindex shader, chrome spring, span-walker borders) onto main.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>",
          "timestamp": "2026-09-07T16:13:49+02:00",
          "tree_id": "07997ec5b86af35450c79e1b95286bc27ec66088",
          "url": "https://github.com/unwndevices/enjin/commit/2e5eb7f9fbd25dee8dcaba86f56f02b682312922"
        },
        "date": 1788790479861,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 22,
            "range": "± 4.35%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 66,
            "range": "± 1.54%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 121,
            "range": "± 2.54%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 228.5,
            "range": "± 0.66%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 69834,
            "range": "± 2.91%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 22,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 636,
            "range": "± 0.16%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 5161.5,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 240,
            "range": "± 1.04%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 612,
            "range": "± 2.3%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1122.0027,
            "range": "± 1.6%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2203.5,
            "range": "± 1.27%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3102.5,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 70,
            "range": "± 1.45%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 79.5,
            "range": "± 0.63%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 28,
            "range": "± 1.72%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 53,
            "range": "± 1.92%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 83,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 144,
            "range": "± 0.69%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 214,
            "range": "± 0.47%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 50400,
            "range": "± 2.79%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 768,
            "range": "± 6.07%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1361,
            "range": "± 3.54%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2091,
            "range": "± 3.34%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2043.5,
            "range": "± 1.26%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1134.5,
            "range": "± 4.9%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 2775.5,
            "range": "± 0.27%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "62b63341094038ac803c9e9db64b97f45755f0fe",
          "message": "merge: physics C_Body/ColliderSet (#80) into scene-panel integ",
          "timestamp": "2026-09-10T19:55:14+02:00",
          "tree_id": "d3ee2d4082d1c475ebed36eec5a46cfc81f8e904",
          "url": "https://github.com/unwndevices/enjin/commit/62b63341094038ac803c9e9db64b97f45755f0fe"
        },
        "date": 1789063056016,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 281,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84563,
            "range": "± 0.14%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 6542,
            "range": "± 0.61%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (5 layers)",
            "value": 38482,
            "range": "± 0.37%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 291,
            "range": "± 3.0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 791,
            "range": "± 1.19%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1453,
            "range": "± 0.68%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2735.5,
            "range": "± 0.39%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3957,
            "range": "± 0.51%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 10.0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0.83%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 4.09%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 58109,
            "range": "± 3.01%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 967,
            "range": "± 3.62%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1743.5,
            "range": "± 2.24%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2690,
            "range": "± 3.2%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2534,
            "range": "± 1.79%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1488,
            "range": "± 5.09%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3877.5,
            "range": "± 0.9%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "31801f31413d9a32a69aa6e4f69a8781bad2b22b",
          "message": "merge: import pipeline, layered sprites (#87-#100) and PSRAM Lua heap",
          "timestamp": "2026-09-13T12:01:59+02:00",
          "tree_id": "2116e5ccbce2440bac7af2b9246e83c0fedd1516",
          "url": "https://github.com/unwndevices/enjin/commit/31801f31413d9a32a69aa6e4f69a8781bad2b22b"
        },
        "date": 1789293801529,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 281,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84669,
            "range": "± 0.14%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 6421.5,
            "range": "± 0.46%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (5 layers)",
            "value": 39354,
            "range": "± 0.31%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 300,
            "range": "± 0.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 790.6785,
            "range": "± 1.11%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1442,
            "range": "± 0.69%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2705,
            "range": "± 0.74%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3997,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 1.1%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 31,
            "range": "± 6.9%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 65924,
            "range": "± 3.23%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 1017.5,
            "range": "± 4.68%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1693.5,
            "range": "± 3.39%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2595,
            "range": "± 2.81%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2594.5,
            "range": "± 2.88%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1528.5,
            "range": "± 3.64%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 4059.1559,
            "range": "± 0.55%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "845b297dde4a2b8a61e2d39da8ceefe340ace831",
          "message": "feat(graphics): generic skin-pack loader seam (Tomodachi #154)\n\nA reusable engine seam that parses a skin.lua folder pack — mandatory\n15-hex luminance-ordered palette + optional named .njn assets + nineSlice\ncorners — into a LayeredAssetStore's AssetArena, via the same sandboxed\nreturn-a-table Lua reader as applet manifest.lua. Knows no player\nvocabulary; that stays in the consumer.\n\nThe manifest reader needs Lua, so skin_pack.cpp compiles in enjin2_lua\n(enjin2_graphics stays Lua-free). SkinPack carries palette + name->handle\nlookup + per-asset corners; applySkinPalette is the live-switch palette\nstep (caller rebuilds the RGB565 LUT and redraws).\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>",
          "timestamp": "2026-09-17T12:30:15+02:00",
          "tree_id": "a3cdcde62dc4bab5713056107c6aedff0f8b6c84",
          "url": "https://github.com/unwndevices/enjin/commit/845b297dde4a2b8a61e2d39da8ceefe340ace831"
        },
        "date": 1790153010775,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 27,
            "range": "± 3.71%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 79,
            "range": "± 1.28%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 143,
            "range": "± 2.05%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 272,
            "range": "± 0.37%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 80828.5,
            "range": "± 0.18%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 25,
            "range": "± 3.85%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 717,
            "range": "± 0.14%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 6026,
            "range": "± 0.3%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (5 layers)",
            "value": 41198.5,
            "range": "± 0.9%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 277,
            "range": "± 4.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 813,
            "range": "± 2.1%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1378.7153,
            "range": "± 1.3%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2574.5,
            "range": "± 0.97%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3661.5,
            "range": "± 0.85%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 84,
            "range": "± 1.2%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 91,
            "range": "± 1.1%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 34,
            "range": "± 3.03%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 63,
            "range": "± 1.56%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 99,
            "range": "± 1.0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 175.5,
            "range": "± 0.85%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 254,
            "range": "± 0.4%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 54441.5,
            "range": "± 1.87%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 919,
            "range": "± 3.03%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1525.5,
            "range": "± 5.12%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2591,
            "range": "± 2.06%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2317,
            "range": "± 5.31%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1355,
            "range": "± 2.92%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3465,
            "range": "± 0.57%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "67eacf9507e7974481ec989ef2b372bc8f21cfe3",
          "message": "fix(graphics): exact-midpoint sagged line, overflow-safe curves + lines (Tomodachi #226)\n\nReview follow-ups for drawQuadBezier / drawSaggedLine.\n\n- drawSaggedLine puts the control point over the chord's exact midpoint\n  instead of rounding it toward (x0,y0). With the control point there, x\n  moves evenly along the curve, so the Span is the graph\n  chord + 4*sag*k*(w-k)/w^2 and is drawn column by column on exact\n  integer terms, using drawLine's own decisions on the exact curve:\n  - odd-width level Spans droop symmetrically (was 195/290 asymmetric);\n  - Sag 0 is drawLine, and more Sag only moves pixels down, steep chords\n    included (was 67/3320 chords popping up from Sag 0 to 1);\n  - a curve that turns back is walked from both ends toward the turn, and\n    the two halves drop an \"L\" against each other's end;\n  - a vertical chord draws drawLine at any Sag instead of running past\n    its lower end.\n- The corner filter also drops a one-pixel spur (the next pixel touches\n  the one before the last, not only diagonally), where a curve dips less\n  than a pixel past an end and comes back.\n- drawLine's error terms are 32-bit, so a span longer than 32767 px ends\n  (it looped forever); drawQuadBezier's chord test is 64-bit. Pixels far\n  outside int16_t are skipped instead of wrapping onto the canvas.\n- Cleanups: one helper for the turn-point formula, the pixel rule stated\n  in words, readable half-walk state, no pixel drawn twice at the join.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>",
          "timestamp": "2026-09-27T12:53:06+02:00",
          "tree_id": "bf996d82d8a50c7669a9b70e30cd30164d3128da",
          "url": "https://github.com/unwndevices/enjin/commit/67eacf9507e7974481ec989ef2b372bc8f21cfe3"
        },
        "date": 1790506512263,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 281,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84578,
            "range": "± 0.17%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 6432,
            "range": "± 0.47%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (5 layers)",
            "value": 38387,
            "range": "± 0.3%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 291,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 791,
            "range": "± 1.15%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1452,
            "range": "± 0.73%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2695.5,
            "range": "± 0.77%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3998,
            "range": "± 0.49%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 1.1%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 2.28%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 60363.5,
            "range": "± 1.63%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 986.5,
            "range": "± 4.23%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1788,
            "range": "± 6.41%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2685,
            "range": "± 2.43%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2520,
            "range": "± 1.41%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1543,
            "range": "± 5.22%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 4098,
            "range": "± 0.27%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "adb30d3dd692aef28f3b4cf4e9e99ea7a0c94b2a",
          "message": "feat(palette): the tape-deck green ramp is the system default\n\nDEFAULT_COLORS is now Tomodachi's music-player green ramp: 15 greens from\ndarkest (0) to lightest (14). The hue-ordered tomo_tune2 palette, the old\ndefault, stays selectable as the \"tomo\" preset for art drawn against its\nindices. palette_test pins the ramp and the preset.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>",
          "timestamp": "2026-09-27T18:40:34+02:00",
          "tree_id": "db9f097119a354cb49a85287df08e8972c1325be",
          "url": "https://github.com/unwndevices/enjin/commit/adb30d3dd692aef28f3b4cf4e9e99ea7a0c94b2a"
        },
        "date": 1790527294551,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 281,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84437,
            "range": "± 0.02%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 992,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 6502,
            "range": "± 0.46%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (5 layers)",
            "value": 38632,
            "range": "± 0.19%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 291,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 781.5,
            "range": "± 1.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1443,
            "range": "± 0.65%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2785,
            "range": "± 0.74%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3997,
            "range": "± 1.25%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 10.56%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 91,
            "range": "± 1.11%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 8.04%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 2.28%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 62972,
            "range": "± 5.04%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 1002,
            "range": "± 4.0%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1718,
            "range": "± 2.9%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2635,
            "range": "± 2.46%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2610,
            "range": "± 3.15%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1613,
            "range": "± 3.39%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 4068,
            "range": "± 0.48%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "6ce38e9d46832356b04ca2eb807e152052dffeb0",
          "message": "feat(scripting): use palette indices and retire legacy input (#252, #253)",
          "timestamp": "2026-09-30T17:04:27+02:00",
          "tree_id": "cb8f87f4c4af71aacacb65fcebe35292a2d0d413",
          "url": "https://github.com/unwndevices/enjin/commit/6ce38e9d46832356b04ca2eb807e152052dffeb0"
        },
        "date": 1790780730787,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130.5,
            "range": "± 0.38%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 291,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 81503,
            "range": "± 0.02%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 851,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 6981,
            "range": "± 0.43%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (5 layers)",
            "value": 38088,
            "range": "± 0.1%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 281,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 791,
            "range": "± 1.25%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1452,
            "range": "± 0.68%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2764,
            "range": "± 0.56%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 4061.5,
            "range": "± 0.86%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 100,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0.7%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 230,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 331,
            "range": "± 0.3%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 54322,
            "range": "± 5.4%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 996,
            "range": "± 6.24%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1818,
            "range": "± 3.41%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2598.5,
            "range": "± 1.74%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2574,
            "range": "± 3.11%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1527.5,
            "range": "± 2.92%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 4557,
            "range": "± 0.44%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "ee8201b77aabb39d1169de4329d667fb49013003",
          "message": "Merge remote-tracking branch 'origin/main'",
          "timestamp": "2026-09-30T17:07:49+02:00",
          "tree_id": "02e2a538a309c37795ed1a6135a43042d088e23b",
          "url": "https://github.com/unwndevices/enjin/commit/ee8201b77aabb39d1169de4329d667fb49013003"
        },
        "date": 1790780969119,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 20,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 100,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 230,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 63401,
            "range": "± 0.13%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 20,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 661,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 5 layers",
            "value": 5468,
            "range": "± 0.37%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (5 layers)",
            "value": 29625,
            "range": "± 0.07%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 220,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 611,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1122,
            "range": "± 0.9%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2163,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3165,
            "range": "± 0.63%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 80,
            "range": "± 0.62%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 60,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 100,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 180,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 261,
            "range": "± 0.38%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 42033.5,
            "range": "± 2.57%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 721,
            "range": "± 4.8%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1262,
            "range": "± 3.58%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2008,
            "range": "± 3.24%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 1842.5,
            "range": "± 2.48%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1161.5,
            "range": "± 5.4%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3495.5,
            "range": "± 0.43%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "92daf4dc80f93842d8dd2e62cf3bf727e7d22f28",
          "message": "feat(lua): reserve UI layer and expose three applet layers (#254)",
          "timestamp": "2026-09-30T18:04:37+02:00",
          "tree_id": "b895c9992b91c34f1caf160158628bc559dfb27b",
          "url": "https://github.com/unwndevices/enjin/commit/92daf4dc80f93842d8dd2e62cf3bf727e7d22f28"
        },
        "date": 1790784343486,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 19,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 56,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 100,
            "range": "± 1.96%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 199,
            "range": "± 1.53%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 61552.5,
            "range": "± 0.07%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 19,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 822,
            "range": "± 0.49%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 4 layers",
            "value": 3537,
            "range": "± 0.54%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (4 layers)",
            "value": 22320,
            "range": "± 0.81%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 160,
            "range": "± 1.91%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 500,
            "range": "± 1.2%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 935.5,
            "range": "± 1.17%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 1765,
            "range": "± 0.51%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 2597.5,
            "range": "± 1.26%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 62,
            "range": "± 1.61%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 70.5,
            "range": "± 2.13%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 24,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 48,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 76,
            "range": "± 1.3%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 133,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 189,
            "range": "± 0.53%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 39990.5,
            "range": "± 2.33%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 645,
            "range": "± 6.26%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1373,
            "range": "± 14.2%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2521,
            "range": "± 2.89%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2425,
            "range": "± 5.95%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1404,
            "range": "± 4.54%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3537,
            "range": "± 3.77%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "95d6c45e32a36db315eaed85a039cb65101ac573",
          "message": "fix(scripting): setPaletteColor r,g,b form, optional clearLayer layer, line width (Tomodachi #255)\n\n- gfx.setPaletteColor(i, r, g, b) works again: lua_isstring accepted the\n  number r, so the r,g,b form was parsed as a hex string.\n- gfx.clearLayer's layer is optional; without it the current layer clears.\n- gfx.setLineWidth feeds gfx.line and the \"line\" modes of rectangle, circle\n  and triangle. Rectangle and circle outlines grow inward (the rectangle is\n  the solid border stroke); lines and triangle edges use a centred square\n  pen. The width is clamped to 1..255.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>",
          "timestamp": "2026-09-30T18:05:59+02:00",
          "tree_id": "b9ab8bd7c738cce5f3e10c0064e81b82b19ed208",
          "url": "https://github.com/unwndevices/enjin/commit/95d6c45e32a36db315eaed85a039cb65101ac573"
        },
        "date": 1790784422450,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 22,
            "range": "± 4.35%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 73,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 121,
            "range": "± 2.42%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 232.5,
            "range": "± 0.65%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 71747,
            "range": "± 0.71%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 22,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 652,
            "range": "± 0.15%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 4 layers",
            "value": 4212,
            "range": "± 0.24%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (4 layers)",
            "value": 23325.5,
            "range": "± 0.68%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 233.5,
            "range": "± 3.32%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 633,
            "range": "± 1.58%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1135,
            "range": "± 0.92%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2122,
            "range": "± 1.46%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3065.5,
            "range": "± 1.03%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 70.5,
            "range": "± 3.68%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 81,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 28,
            "range": "± 3.58%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 52,
            "range": "± 0.94%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 85.5,
            "range": "± 0.59%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 146,
            "range": "± 0.68%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 205,
            "range": "± 0.49%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 50009,
            "range": "± 1.43%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 787,
            "range": "± 4.27%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1376,
            "range": "± 5.64%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 1998.5,
            "range": "± 3.76%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2134.5,
            "range": "± 3.54%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1176.5,
            "range": "± 2.98%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 2773,
            "range": "± 0.32%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "fe59f4a7ad48a53d73d4934abc67524caa66ddc8",
          "message": "feat(scripting): scene-free engine.tilemap.load map handle (Tomodachi #256)\n\nengine.tilemap.load(name) now returns a Lua-owned map handle instead of\nspawning an object in the active scene. The handle is a full userdata that\nis a C_Tilemap with no owner Object; __gc runs its destructor. It shares the\nC_Tilemap methods (attrAt, sweepAabb, ...), and colliders:addSolidRects\ntakes it too.\n\ntilemap_lua.hpp is the one header-only lookup (toTilemap / checkTilemap)\nfor a handle or a scene C_Tilemap proxy, so a host that does not link\nLuaBindings (libtomo's gfx.setTilemap) resolves a map argument the same way.\nC_Drawable / C_Tilemap tolerate a null owner.\n\nUnchanged: tileset pixels share the 64 KiB asset arena and hold a sprite-pool\nslot until resetSpritePool(); maps larger than 64x64 are cropped.\n\nTests: asset_loader_test loads a 16x16 .njn + .njm fixture with no scene,\nchecks attrAt / sweepAabb and the drawn pixels, and collects the handle.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>",
          "timestamp": "2026-09-30T19:13:57+02:00",
          "tree_id": "43d9eddb575e7ba94246ac7e0e840f68f0e98abc",
          "url": "https://github.com/unwndevices/enjin/commit/fe59f4a7ad48a53d73d4934abc67524caa66ddc8"
        },
        "date": 1790788501932,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 280,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84387,
            "range": "± 0.01%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 1002,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 4 layers",
            "value": 5380,
            "range": "± 0.55%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (4 layers)",
            "value": 31373,
            "range": "± 0.65%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 301,
            "range": "± 0.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 782,
            "range": "± 1.14%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1453,
            "range": "± 0.69%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2724.3792,
            "range": "± 1.12%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3957,
            "range": "± 0.74%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 10.0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 1.1%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 3.45%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 220,
            "range": "± 4.27%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 62457,
            "range": "± 5.11%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 1007,
            "range": "± 2.95%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1698,
            "range": "± 3.14%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2710.5,
            "range": "± 2.21%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2604.5,
            "range": "± 1.76%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1473,
            "range": "± 4.76%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3847,
            "range": "± 0.51%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "c89814aae5f52ed63339b52f49b1def541d41b76",
          "message": "fix(graphics): skin-pack manifest VM prefers PSRAM, protected setup (Tomodachi #277)\n\nloadSkinPack built its throwaway VM with luaL_newstate() + luaL_openlibs(). On\nthe ESP32-S3 plain malloc is internal RAM only, and with TinyUSB + BLE up there\nwas ~38 KB left before the Music Player launched: the VM ran internal RAM dry,\nand luaL_openlibs raised the OOM outside any pcall, so it aborted the device.\n\n- allocate the VM through a PSRAM-first allocator on ESP32 (the applet VM\n  already does, via LuaPlatform::createState);\n- open only the base library: the reader needs load/assert/type, and the\n  manifest itself runs in an empty environment;\n- open it and run the reader inside lua_pcall, so running out of memory fails\n  the load (ROM skin fallback) instead of aborting.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>",
          "timestamp": "2026-09-30T20:04:56+02:00",
          "tree_id": "7638246df5d1e7a3311fdc61a7b71c5ca63022af",
          "url": "https://github.com/unwndevices/enjin/commit/c89814aae5f52ed63339b52f49b1def541d41b76"
        },
        "date": 1790791548374,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 280,
            "range": "± 0.36%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84399,
            "range": "± 0.01%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 1002,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 4 layers",
            "value": 5380,
            "range": "± 0.55%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (4 layers)",
            "value": 31014,
            "range": "± 0.34%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 300,
            "range": "± 0.33%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 791,
            "range": "± 1.2%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1453,
            "range": "± 0.69%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2746,
            "range": "± 1.11%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 4037,
            "range": "± 0.51%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 0.55%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 220,
            "range": "± 0.45%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 62438,
            "range": "± 4.27%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 1037,
            "range": "± 5.26%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1738.5,
            "range": "± 3.92%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2690,
            "range": "± 1.89%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2580,
            "range": "± 2.58%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1518,
            "range": "± 3.05%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3812,
            "range": "± 0.78%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "961e7a4e07e3b62dadc39b65964a4a2fb4cb925f",
          "message": "feat(scripting): Lua API descriptors register and document the core bindings (Tomodachi #257)\n\nEach binding's descriptor array is now its registration input (ADR-0013):\nan entry holds the name and C function (or constant / nested table /\nlifecycle callback), a signature string, a one-line summary, one line per\nargument, an optional host note, and the named enums the binding matches its\narguments against. The arrays are static constexpr, so they sit in flash.\n\n- lua_api.hpp: descriptor types, registration helpers (luaApiSetFields /\n  SetSubtable / SetGlobalTable / SetGlobals) that record each module in a\n  per-VM registry (luaApiModules), and the signature grammar.\n- lua_api_signature.cpp: parser, canonical formatter, arg-line split and\n  validateLuaApiModule (signatures parse, arg lines match the parameters,\n  lowercase types are builtins, enums are referenced).\n- Converted: the gfx block and print in registerAll(), gfx.remap/mask/effect\n  (effect_lua.hpp), bindings_math.cpp (globals, Vec2/Point/Rect metatables\n  and methods), engine.tween, engine.async and engine.ui.\n- Enum name arrays (easings, style slots, palette presets, alignments, shape\n  modes, theme, remap/mask/effect kinds) are matched through nameIndex(), so\n  the documented and accepted names are one array.\n- lua_api_test: grammar round-trip and malformed strings, exact-entries\n  registration, the registry, validation, and every key the converted\n  surfaces put in Lua is described.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>",
          "timestamp": "2026-09-30T21:02:15+02:00",
          "tree_id": "b0382d52137db363610dc790795c984db297bdf9",
          "url": "https://github.com/unwndevices/enjin/commit/961e7a4e07e3b62dadc39b65964a4a2fb4cb925f"
        },
        "date": 1790795000657,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 140,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 300,
            "range": "± 0.33%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84047,
            "range": "± 0.04%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 851,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 4 layers",
            "value": 5829,
            "range": "± 0.53%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (4 layers)",
            "value": 30937,
            "range": "± 0.18%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 280.5,
            "range": "± 0.18%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 791,
            "range": "± 0.94%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1442,
            "range": "± 1.35%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2764.5,
            "range": "± 0.71%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 4047,
            "range": "± 0.74%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 90,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 100,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 80,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 230,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 331,
            "range": "± 0.3%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 52870,
            "range": "± 2.65%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 906,
            "range": "± 5.17%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1647,
            "range": "± 2.49%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2509,
            "range": "± 2.68%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 2373.5,
            "range": "± 2.15%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1452.5,
            "range": "± 2.87%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 4867.5,
            "range": "± 0.81%",
            "unit": "ns/op"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "committer": {
            "email": "ciro@unwn.dev",
            "name": "Ciro Caputo Viglione",
            "username": "unwndevices"
          },
          "distinct": true,
          "id": "c97e0c5359143668de79be5d1c42ce8a8b07d416",
          "message": "feat(scripting): engine.* registers from descriptors, behind per-feature switches (Tomodachi #258)\n\nThe rest of the enjin core surface now registers from descriptor arrays\n(ADR-0013), and the parts a Tomodachi host does not run are switchable.\n\n- LuaFeatures {scene, camera, debug, raycast, proxies}, all off by default,\n  set with LuaBindings::setFeatures() before registerAll(). Each switch is\n  checked once. LuaFeatures::all() is the full surface; the SDL/headless\n  mains, C_LuaScript and the tests that exercise those features opt in.\n- engine.* in bindings_engine.cpp: time, collision, lua, random, store,\n  sprite, tilemap, event, camera, config, state, physics (+ a switchable\n  raycast module), scene and log. engine.debug (bindings_debug.cpp) and\n  engine.hud (bindings_numerals.cpp) too; RollingCounter/Timer metatables in\n  hud_lua.hpp, timer modes matched by nameIndex(kTimerModeNames).\n- engine.graphics re-exports gfx through constexpr luaApiAlias(kGfx, name):\n  a misspelt name fails to compile.\n- Tilemap handle: the strcmp __index is gone; kTilemapMethodsModule is\n  registered from registerEngineTable whatever the switches say.\n- Proxies: ScriptProxy, ObjectProxy and the component proxies. Types with\n  properties keep an __index function over a registry methods table\n  (setProxyMethods / pushProxyMethod); methods-only types index a methods\n  table, so a stale proxy now errors on call rather than on index. Sprite\n  loop modes use kLoopModeNames.\n- lua_api_test: engine.* described, defaults off, each switch alone\n  registers only its own feature, every proxy described, graphics aliases\n  are gfx's entries, a map handle works with the proxies off.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>",
          "timestamp": "2026-09-30T21:28:47+02:00",
          "tree_id": "5db339745cfd5db9105d01bd6167134de628a8fe",
          "url": "https://github.com/unwndevices/enjin/commit/c97e0c5359143668de79be5d1c42ce8a8b07d416"
        },
        "date": 1790796580352,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "canvas4: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: clear",
            "value": 130,
            "range": "± 0.76%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: fillRect 32x32",
            "value": 130,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: drawCircle r16",
            "value": 281,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas4: blit 128x128 sprite",
            "value": 84567.5,
            "range": "± 0.02%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: setPixel",
            "value": 30,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "canvas8: fillRect 32x32",
            "value": 1002,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: composite 4 layers",
            "value": 5380,
            "range": "± 0.56%",
            "unit": "ns/op"
          },
          {
            "name": "compositor: compositeDirty full-frame (4 layers)",
            "value": 31023,
            "range": "± 0.31%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x1",
            "value": 291,
            "range": "± 3.0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x8",
            "value": 791,
            "range": "± 1.15%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x16",
            "value": 1472,
            "range": "± 0.74%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x32",
            "value": 2734.5,
            "range": "± 0.72%",
            "unit": "ns/op"
          },
          {
            "name": "scene::addObject x48",
            "value": 3967,
            "range": "± 0.38%",
            "unit": "ns/op"
          },
          {
            "name": "object::addComponent<C_Position>",
            "value": 81,
            "range": "± 1.25%",
            "unit": "ns/op"
          },
          {
            "name": "object::removeComponent<C_Position>",
            "value": 91,
            "range": "± 1.11%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x1 objects",
            "value": 40,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x8 objects",
            "value": 70,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x16 objects",
            "value": 120,
            "range": "± 0%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x32 objects",
            "value": 211,
            "range": "± 0.48%",
            "unit": "ns/op"
          },
          {
            "name": "scene::update x48 objects",
            "value": 311,
            "range": "± 0.32%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: init+shutdown",
            "value": 66504,
            "range": "± 5.01%",
            "unit": "ns/op"
          },
          {
            "name": "lua engine: executeString (noop script)",
            "value": 977,
            "range": "± 3.14%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: engine.time.delta call",
            "value": 1698,
            "range": "± 4.99%",
            "unit": "ns/op"
          },
          {
            "name": "lua binding: math.clamp call",
            "value": 2685,
            "range": "± 2.68%",
            "unit": "ns/op"
          },
          {
            "name": "lua proxy: find+field round-trip",
            "value": 3061,
            "range": "± 2.24%",
            "unit": "ns/op"
          },
          {
            "name": "lua event: emit dispatch",
            "value": 1483,
            "range": "± 4.36%",
            "unit": "ns/op"
          },
          {
            "name": "lua GC: full collect",
            "value": 3669.6559,
            "range": "± 0.68%",
            "unit": "ns/op"
          }
        ]
      }
    ]
  }
}