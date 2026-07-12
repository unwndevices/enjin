window.BENCHMARK_DATA = {
  "lastUpdate": 1783875457447,
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
      }
    ]
  }
}