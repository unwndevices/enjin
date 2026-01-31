# Manual Testing Checklist for enjin2

**Phase:** 04-Validation
**Purpose:** Verify critical paths through manual testing
**Last Updated:** 2026-01-31

---

## Component Lifecycle Tests

### Test 1: Awake Order Verification

**Objective:**
Verify Awake() methods execute in correct order when objects are added to scene.

**Test:**
1. Create a scene with 3 objects (Object1, Object2, Object3)
2. Add a component to each object that logs when Awake() is called
3. Add objects to scene in order: Object1 → Object2 → Object3
4. Observe console output for Awake execution order

**Expected:**
Components log Awake() in the order they were added to the scene (Object1, Object2, Object3).

---

### Test 2: Start Order Verification

**Objective:**
Verify Start() methods execute after all Awake() calls complete.

**Test:**
1. Create a scene with 3 objects
2. Add a component to each object that logs both Awake() and Start() calls
3. Add objects to scene
4. Activate the scene
5. Observe console output for Awake and Start execution order

**Expected:**
All Awake() calls complete first, then all Start() calls execute. Start() runs once per scene activation.

---

### Test 3: Update Execution

**Objective:**
Verify Update() method executes on each frame.

**Test:**
1. Create a component with a frame counter that increments in Update()
2. Add component to an object in a scene
3. Run scene for 60 frames
4. Log frame count every 10 frames
5. Check final frame count

**Expected:**
Update() called each frame with deltaTime. Final frame count equals 60.

---

## Rendering Tests

### Test 4: Basic Shape Rendering

**Objective:**
Verify basic shapes (rectangle, circle, line) render correctly.

**Test:**
1. Create a canvas (128x64)
2. Draw rectangle at position (10, 10) with size 50x30
3. Draw circle at center (64, 32) with radius 15
4. Draw line from (10, 50) to (50, 50)
5. Export canvas to BMP
6. Open BMP in image viewer and inspect

**Expected:**
Shapes appear at correct positions with correct colors (white on black background).

---

### Test 5: Layer Ordering

**Objective:**
Verify draw layer (z-order) affects rendering order.

**Test:**
1. Create three overlapping objects (rectangle, circle, line)
2. Set their layers: Object1 layer=1, Object2 layer=2, Object3 layer=3
3. Position them so they overlap at center
4. Export canvas to BMP
5. Verify layering order in image viewer

**Expected:**
Higher layer numbers draw on top (Object3 on top of Object2 on top of Object1).

---

## Scene Transition Tests

### Test 6: Scene Push Behavior

**Objective:**
Verify scene push preserves current scene.

**Test:**
1. Create SceneA with a unique object (e.g., rectangle)
2. Create SceneB with a different object (e.g., circle)
3. Add SceneA to scene manager and activate
4. Push SceneB onto scene stack
5. Verify SceneA still exists but is not active
6. Verify SceneB is active and renders

**Expected:**
Two scenes in stack, top scene (SceneB) is active and rendering, bottom scene (SceneA) still exists.

---

### Test 7: Scene Pop Behavior

**Objective:**
Verify scene pop destroys top scene and restores previous.

**Test:**
1. Push SceneA, then push SceneB onto scene stack
2. Verify SceneB is active
3. Pop scene from stack
4. Verify SceneB is destroyed
5. Verify SceneA becomes active and renders

**Expected:**
Top scene (SceneB) destroyed, previous scene (SceneA) becomes active.

---

## Lua Scripting Tests

### Test 8: Basic Script Execution

**Objective:**
Verify Lua scripts can be loaded and executed.

**Test:**
1. Create Lua script file (test.lua) with:
   ```lua
   print("Lua script executed successfully")
   ```
2. Initialize LuaEngine
3. Load and execute test.lua
4. Observe console output

**Expected:**
Script executes and prints "Lua script executed successfully" to console.

---

## Test Execution Notes

### Pre-requisites
- enjin2 must be built successfully
- Test artifacts directory should be clean before each run
- BMP export functionality must be working (from 04-01)

### Test Artifacts
- Each test should export canvas output to BMP for visual verification
- Save BMPs with descriptive names: `test-01-awake-order.bmp`, etc.
- Log test results to console

### Test Organization
- Tests are organized chronologically: Test 1, Test 2, ..., Test 8
- Execute tests in order to verify component lifecycle first
- Document any failures with specific error messages

---

## Manual Test Execution

To run all manual tests, use the `manual-test.sh` script:

```bash
cd .planning/phases/04-validation
./manual-test.sh
```

The script will:
- Create timestamped result directory
- Execute each test scenario
- Export BMP artifacts
- Generate summary report
