---
id: AnimationTrack
title: AnimationTrack
sidebar_label: AnimationTrack
---

# AnimationTrack

Template animation track for keyframe-based animations. 


TValue type (, float, , etc.) PointPixel4KeyframeTypeKeyframe type (, , etc.) PositionKeyframeFloatKeyframe

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

### `T getCurrentValue() const const`

Get current animation value. 

Current interpolated value 

---

### `AnimationState getState() const const`

Get animation state. 

Current animation state 

---

### `uint16_t getCurrentTime() const const`

Get current time. 

Current playback time in milliseconds 

---

### `uint16_t getDuration() const const`

Get animation duration. 

Total duration in milliseconds 

---

### `float getProgress() const const`

Get normalized progress (0.0 to 1.0). 

Animation progress 

---

### `SignalConnection connectOnStart(std::function&lt; void()&gt; callback)`

Connect to animation start event. 

callbackFunction to call when animation starts  connection for disconnecting callback Signal

---

### `SignalConnection connectOnComplete(std::function&lt; void()&gt; callback)`

Connect to animation complete event. 

callbackFunction to call when animation completes  connection for disconnecting callback Signal

---

### `&lt; T &gt;SignalConnection connectOnUpdate(std::function&lt; void(T)&gt; callback)`

Connect to animation update event. 

callbackFunction to call when animation value updates  connection for disconnecting callback Signal

---

## Private Methods

### `void handleLoopBoundary()`

 reaching loop boundary. Handle


        

---

### `T evaluateAtTime(uint16_t time) const const`

Evaluate animation value at specific time. 

timeTime to evaluate at Interpolated value at time 

---

### `T getValue(const KeyframeType &keyframe) const const`

Get value from keyframe (specialized for each keyframe type). 


        

---

### `T interpolateBetween(const KeyframeType &from, const KeyframeType &to, uint16_t time) const const`

Interpolate between two keyframes. 


        

---

### `Point getValue(const PositionKeyframe &keyframe) const const`


        


        

---

### `float getValue(const FloatKeyframe &keyframe) const const`


        


        

---

### `Pixel4 getValue(const ColorKeyframe &keyframe) const const`


        


        

---

