// Shared M2 test fixture (unwn #183): the DatumManagerScene behavior port.
//
// The ratified prototype (eisei/tools/scene_data_prototype/scene_datum_manager.json)
// transcribed onto the C++ schema: entity ids are IdComponent, entity props are
// reflected field names (selectedIndex, not selectionIndex), non-ECS visuals are
// `cpp:` slots with opaque bags, and the enter animation drives real reflected
// numeric fields (overlay opacity, gauge value). Field-level shape is
// v1-provisional per the locked spec — this file is the C++-side strawman.
#pragma once

#include <enjin2/ui/components.hpp>
#include <enjin2/ui/slot.hpp>
#include <enjin2/ui/widgets/gauge.hpp>
#include <enjin2/ui/widgets/icon.hpp>
#include <enjin2/ui/widgets/label.hpp>
#include <enjin2/ui/widgets/list.hpp>
#include <enjin2/ui/widgets/overlay.hpp>
#include <enjin2/ui/widgets/popup.hpp>
#include <enjin2/ui/world.hpp>

using SceneVmWorld =
    enjin2::World<16, enjin2::IdComponent, enjin2::PositionComponent, enjin2::SizeComponent,
                  enjin2::LabelComponent, enjin2::IconComponent, enjin2::GaugeComponent,
                  enjin2::OverlayComponent, enjin2::PopUpComponent, enjin2::ListComponent,
                  enjin2::SlotComponent, enjin2::BindingsComponent>;

static const char* const kDatumManagerScene = R"json({
  "version": 2,
  "scene": "datum_manager",
  "state": {
    "isLoading": false,
    "listReady": false,
    "previewInFlight": false,
    "showMiniPreview": false,
    "showProgressBar": false,
    "previewProgress": 0.0,
    "currentSlot": 2
  },
  "entities": [
    {
      "components": {
        "id": { "id": "presetList" },
        "position": { "position": { "x": 63, "y": 94 } },
        "size": { "size": { "width": 70, "height": 74 } },
        "list": { "items": ["-----"], "itemSpacing": 3, "selectedIndex": 0 }
      }
    },
    {
      "components": {
        "id": { "id": "statusLabel" },
        "position": { "position": { "x": 2, "y": 25 } },
        "size": { "size": { "width": 124, "height": 16 } },
        "label": { "text": "" }
      }
    },
    {
      "components": {
        "id": { "id": "dimmer" },
        "overlay": { "opacity": 0, "visible": true }
      }
    },
    {
      "components": {
        "id": { "id": "meter" },
        "position": { "position": { "x": 8, "y": 40 } },
        "gauge": { "diameter": 40, "mode": 1, "value": -1.0 }
      }
    },
    {
      "components": {
        "id": { "id": "previewBands" },
        "slot": {
          "slot": "DatumBandsPreview",
          "props": { "bands": 20, "barW": 3, "gap": 1, "maxBarH": 40, "baseline": 62, "visible": false }
        },
        "bindings": { "bindings": { "visible": "showMiniPreview" } }
      }
    },
    {
      "components": {
        "id": { "id": "progressBar" },
        "slot": {
          "slot": "ProgressBar",
          "props": { "x": 23, "y": 46, "w": 80, "h": 4, "visible": false, "progress": 0.0 }
        },
        "bindings": { "bindings": { "visible": "showProgressBar", "progress": "previewProgress" } }
      }
    },
    {
      "components": {
        "id": { "id": "screenMask" },
        "slot": {
          "slot": "CircularMask",
          "props": { "cx": 63, "cy": 63, "r": 60, "borderColor": 12 }
        }
      }
    }
  ],
  "timers": {
    "previewDebounce": {
      "ms": 400,
      "onExpire": [
        { "if": "listReady",
          "do": [
            { "host": "datum.preview.request", "args": { "selection": "@presetList.selectedIndex" } },
            { "set": ["previewInFlight", true] }
          ] }
      ]
    },
    "statusDismiss": {
      "ms": 1000,
      "onExpire": [ { "set": ["statusLabel.text", ""] } ]
    },
    "listRetry": {
      "ms": 500, "repeat": true,
      "onExpire": [ { "if": "!listReady", "do": [ { "host": "datum.list.request" } ] } ]
    }
  },
  "animations": {
    "enter": {
      "tracks": [
        { "target": "dimmer.opacity", "from": 15, "to": 0, "ms": 250, "easing": "outCubic" },
        { "target": "meter.value", "from": -1.0, "to": 0.4, "ms": 200, "easing": "inOutSine" }
      ]
    }
  },
  "on": {
    "scene.activate": [
      { "do": [
        { "set": ["previewInFlight", false] },
        { "set": ["showMiniPreview", false] },
        { "set": ["showProgressBar", false] },
        { "set": ["isLoading", false] },
        { "anim": ["play", "enter"] },
        { "set": ["listReady", false] },
        { "set": ["statusLabel.text", "Requesting list..."] },
        { "host": "datum.list.request" },
        { "timer": ["start", "listRetry"] }
      ] }
    ],
    "scene.deactivate": [
      { "if": "previewInFlight",
        "do": [ { "host": "datum.preview.cancel" }, { "set": ["previewInFlight", false] } ] },
      { "do": [
        { "timer": ["cancel", "previewDebounce"] },
        { "timer": ["cancel", "listRetry"] },
        { "set": ["showProgressBar", false] },
        { "set": ["showMiniPreview", false] }
      ] }
    ],
    "input.encoder.ccw": [
      { "if": "!isLoading && listReady",
        "do": [
          { "call": "presetList.moveUp" },
          { "if": "previewInFlight",
            "do": [ { "host": "datum.preview.cancel" }, { "set": ["previewInFlight", false] } ] },
          { "set": ["showProgressBar", false] },
          { "set": ["showMiniPreview", false] },
          { "timer": ["start", "previewDebounce"] }
        ] }
    ],
    "input.encoder.cw": [
      { "if": "!isLoading && listReady",
        "do": [
          { "call": "presetList.moveDown" },
          { "if": "previewInFlight",
            "do": [ { "host": "datum.preview.cancel" }, { "set": ["previewInFlight", false] } ] },
          { "set": ["showProgressBar", false] },
          { "set": ["showMiniPreview", false] },
          { "timer": ["start", "previewDebounce"] }
        ] }
    ],
    "input.tap.select": [
      { "if": "!isLoading && listReady",
        "do": [
          { "if": "previewInFlight",
            "do": [ { "host": "datum.preview.cancel" }, { "set": ["previewInFlight", false] } ] },
          { "timer": ["cancel", "previewDebounce"] },
          { "host": "datum.load",
            "args": { "slot": "@state.currentSlot", "selection": "@presetList.selectedIndex" } },
          { "set": ["isLoading", true] },
          { "set": ["statusLabel.text", "Loading..."] }
        ] }
    ],
    "input.tap.set": [
      { "do": [ { "sceneSwitch": "control" } ] }
    ],
    "host.listArrived": [
      { "do": [
        { "call": "presetList.setItems", "args": ["@event.names"] },
        { "call": "presetList.setSelection", "args": ["@event.loadedIndex"] },
        { "set": ["listReady", true] },
        { "timer": ["cancel", "listRetry"] },
        { "set": ["statusLabel.text", ""] },
        { "timer": ["start", "previewDebounce"] }
      ] }
    ],
    "host.preview.start": [
      { "do": [ { "set": ["showProgressBar", true] }, { "set": ["previewProgress", 0.0] } ] }
    ],
    "host.preview.progress": [
      { "do": [ { "set": ["showProgressBar", true] }, { "set": ["previewProgress", "@event.progress"] } ] }
    ],
    "host.preview.done": [
      { "do": [
        { "set": ["showProgressBar", false] },
        { "set": ["showMiniPreview", true] },
        { "set": ["previewInFlight", false] }
      ] }
    ],
    "host.preview.error": [
      { "do": [ { "set": ["showProgressBar", false] }, { "set": ["previewInFlight", false] } ] }
    ],
    "host.loadResult.ok": [
      { "do": [
        { "set": ["isLoading", false] },
        { "set": ["statusLabel.text", "Loaded!"] },
        { "timer": ["start", "statusDismiss"] },
        { "set": ["showMiniPreview", true] },
        { "host": "ui.exitScene" }
      ] }
    ],
    "host.loadResult.fail": [
      { "do": [
        { "set": ["isLoading", false] },
        { "set": ["statusLabel.text", "@event.message"] }
      ] }
    ]
  }
})json";
