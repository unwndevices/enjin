---
id: AnimationTrack
title: AnimationTrack
sidebar_label: AnimationTrack
---

# AnimationTrack

Template animation track for keyframe-based animations. 


templateparamTValue type (, float, , etc.) Pointstructenjin2_1_1PointcompoundPixel4structenjin2_1_1Pixel4compoundKeyframeTypeKeyframe type (, , etc.) PositionKeyframestructenjin2_1_1PositionKeyframecompoundFloatKeyframestructenjin2_1_1FloatKeyframecompound

---

**Namespace:** enjin2

**Header:** include/enjin2/animation/animation_track.hpp

## Public Methods

### `cpp
* AnimationTrack()*
``

Constructor. 


        

---

### `cpp
*bool addKeyframe(const KeyframeType &keyframe)*
``

Add a keyframe to the track. 

paramkeyframeKeyframe to add returnTrue if keyframe was added successfully 

---

### `cpp
*void clearKeyframes()*
``

Clear all keyframes. 


        

---

### `cpp
*void play()*
``

Start animation playback. 


        

---

### `cpp
*void pause()*
``

Pause animation playback. 


        

---

### `cpp
*void stop()*
``

Stop animation playback and reset to beginning. 


        

---

### `cpp
*void setLoopMode(LoopMode mode)*
``

Set loop mode. 

parammodeLoop mode to set 

---

### `cpp
*void update(uint16_t deltaTime)*
``

Update animation by time delta. 

paramdeltaTimeTime elapsed since last update in milliseconds 

---

### `cpp
*T getCurrentValue() const const*
``

Get current animation value. 

returnCurrent interpolated value 

---

### `cpp
*AnimationState getState() const const*
``

Get animation state. 

returnCurrent animation state 

---

### `cpp
*uint16_t getCurrentTime() const const*
``

Get current time. 

returnCurrent playback time in milliseconds 

---

### `cpp
*uint16_t getDuration() const const*
``

Get animation duration. 

returnTotal duration in milliseconds 

---

### `cpp
*float getProgress() const const*
``

Get normalized progress (0.0 to 1.0). 

returnAnimation progress 

---

### `cpp
*SignalConnectionclassenjin2_1_1SignalConnectioncompound connectOnStart(std::function&lt; void()&gt; callback)*
``

Connect to animation events. 


        

---

### `cpp
*SignalConnectionclassenjin2_1_1SignalConnectioncompound connectOnComplete(std::function&lt; void()&gt; callback)*
``


        


        

---

### `cpp
*&lt; T &gt;SignalConnectionclassenjin2_1_1SignalConnectioncompound connectOnUpdate(std::function&lt; void(T)&gt; callback)*
``


        


        

---

## Private Methods

### `cpp
*void handleLoopBoundary()*
``

 reaching loop boundary. Handlestructenjin2_1_1Handlecompound


        

---

### `cpp
*T evaluateAtTime(uint16_t time) const const*
``

Evaluate animation value at specific time. 

paramtimeTime to evaluate at returnInterpolated value at time 

---

### `cpp
*T getValue(const KeyframeType &keyframe) const const*
``

Get value from keyframe (specialized for each keyframe type). 


        

---

### `cpp
*T interpolateBetween(const KeyframeType &from, const KeyframeType &to, uint16_t time) const const*
``

Interpolate between two keyframes. 


        

---

### `cpp
*Pointstructenjin2_1_1Pointcompound getValue(const PositionKeyframe &keyframe) const const*
``


        


        

---

### `cpp
*float getValue(const FloatKeyframe &keyframe) const const*
``


        


        

---

### `cpp
*Pixel4structenjin2_1_1Pixel4compound getValue(const ColorKeyframe &keyframe) const const*
``


        


        

---

