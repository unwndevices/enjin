---
id: AnimationTrack
title: AnimationTrack
sidebar_label: AnimationTrack
---

# AnimationTrack

Template animation track for keyframe-based animations. 


templateparamTValue type (, float, , etc.) Pointstructenjin2_1_1PointcompoundPixel4structenjin2_1_1Pixel4compoundKeyframeTypeKeyframe type (, , etc.) PositionKeyframestructenjin2_1_1PositionKeyframecompoundFloatKeyframestructenjin2_1_1FloatKeyframecompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/animation/animation_track.hpp`

## Public Methods

### ` AnimationTrack()`

Constructor. 


        

---

### `bool addKeyframe(const KeyframeType &keyframe)`

Add a keyframe to the track. 

paramkeyframeKeyframe to add returnTrue if keyframe was added successfully 

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

parammodeLoop mode to set 

---

### `void update(uint16_t deltaTime)`

Update animation by time delta. 

paramdeltaTimeTime elapsed since last update in milliseconds 

---

### `T getCurrentValue() const const`

Get current animation value. 

returnCurrent interpolated value 

---

### `AnimationState getState() const const`

Get animation state. 

returnCurrent animation state 

---

### `uint16_t getCurrentTime() const const`

Get current time. 

returnCurrent playback time in milliseconds 

---

### `uint16_t getDuration() const const`

Get animation duration. 

returnTotal duration in milliseconds 

---

### `float getProgress() const const`

Get normalized progress (0.0 to 1.0). 

returnAnimation progress 

---

### `SignalConnectionclassenjin2_1_1SignalConnectioncompound connectOnStart(std::function< void()> callback)`

Connect to animation events. 


        

---

### `SignalConnectionclassenjin2_1_1SignalConnectioncompound connectOnComplete(std::function< void()> callback)`


        


        

---

### `< T >SignalConnectionclassenjin2_1_1SignalConnectioncompound connectOnUpdate(std::function< void(T)> callback)`


        


        

---

## Private Methods

### `void handleLoopBoundary()`

 reaching loop boundary. Handlestructenjin2_1_1Handlecompound


        

---

### `T evaluateAtTime(uint16_t time) const const`

Evaluate animation value at specific time. 

paramtimeTime to evaluate at returnInterpolated value at time 

---

### `T getValue(const KeyframeType &keyframe) const const`

Get value from keyframe (specialized for each keyframe type). 


        

---

### `T interpolateBetween(const KeyframeType &from, const KeyframeType &to, uint16_t time) const const`

Interpolate between two keyframes. 


        

---

### `Pointstructenjin2_1_1Pointcompound getValue(const PositionKeyframe &keyframe) const const`


        


        

---

### `float getValue(const FloatKeyframe &keyframe) const const`


        


        

---

### `Pixel4structenjin2_1_1Pixel4compound getValue(const ColorKeyframe &keyframe) const const`


        


        

---

