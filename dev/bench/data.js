window.BENCHMARK_DATA = {
  "lastUpdate": 1772964470879,
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
      }
    ]
  }
}