---
id: AnimationTrack
title: AnimationTrack
sidebar_label: AnimationTrack
---

# AnimationTrack

Template animation track for keyframe-based animations. 


TValue type (Point, float, Pixel4, etc.) KeyframeTypeKeyframe type (PositionKeyframe, FloatKeyframe, etc.) 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/animation/animation_track.hpp`

## Public Methods

### ` AnimationTrack()`

Constructor. 

---

### `bool addKeyframe(const KeyframeType &keyframe)`

Add a keyframe to the track. 

keyframeKeyframe to add True if keyframe was added successfully 

---

### `void clearKeyframes()`

Clear all keyframes. 

---

### `void play()`

Start animation playback. 

---

### `void pause()`

Pause animation playback. 

---

### `void stop()`

Stop animation playback and reset to beginning. 

---

### `void setLoopMode(LoopMode mode)`

Set loop mode. 

modeLoop mode to set 

---

### `void update(uint16_t deltaTime)`

Update animation by time delta. 

deltaTimeTime elapsed since last update in milliseconds 

---

### `T getCurrentValue() const`

Get current animation value. 

Current interpolated value 

---

### `AnimationState getState() const`

Get animation state. 

Current animation state 

---

### `uint16_t getCurrentTime() const`

Get current time. 

Current playback time in milliseconds 

---

### `uint16_t getDuration() const`

Get animation duration. 

Total duration in milliseconds 

---

### `float getProgress() const`

Get normalized progress (0.0 to 1.0). 

Animation progress 

---

### `SignalConnection connectOnStart(std::function&lt; void()&gt; callback)`

Connect to animation start event. 

callbackFunction to call when animation starts Signal connection for disconnecting callback 

---

### `SignalConnection connectOnComplete(std::function&lt; void()&gt; callback)`

Connect to animation complete event. 

callbackFunction to call when animation completes Signal connection for disconnecting callback 

---

### `SignalConnection&lt; T &gt; connectOnUpdate(std::function&lt; void(T)&gt; callback)`

Connect to animation update event. 

callbackFunction to call when animation value updates Signal connection for disconnecting callback 

---

## Private Methods

### `void handleLoopBoundary()`

Handle reaching loop boundary. 

---

### `T evaluateAtTime(uint16_t time) const`

Evaluate animation value at specific time. 

timeTime to evaluate at Interpolated value at time 

---

### `T getValue(const KeyframeType &keyframe) const`

Get value from keyframe (specialized for each keyframe type). 

---

### `T interpolateBetween(const KeyframeType &from, const KeyframeType &to, uint16_t time) const`

Interpolate between two keyframes. 

---

### `Point getValue(const PositionKeyframe &keyframe) const`

---

### `float getValue(const FloatKeyframe &keyframe) const`

---

### `Pixel4 getValue(const ColorKeyframe &keyframe) const`

---

