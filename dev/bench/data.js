window.BENCHMARK_DATA = {
  "lastUpdate": 1783900247844,
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
      }
    ]
  }
}