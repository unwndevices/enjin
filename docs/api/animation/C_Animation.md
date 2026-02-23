---
id: C_Animation
title: C_Animation
sidebar_label: C_Animation
---

# C_Animation

Animation component for object animations. 


Manages multiple animation tracks for different properties (position, scale, rotation, color, etc.) and applies them to the owning object's components. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/animation.hpp`

## Public Methods

### ` C_Animation(Object *owner)`

Constructor. 

ownerOwner object 

---

### `virtual  ~C_Animation()=default`

Destructor. 


        

---

### `virtual void start() override`

 start - set up animation connections. Component


        

---

### `virtual void update(uint16_t deltaTime) override`

 update - update all active tracks. Component

deltaTimeTime elapsed since last update in milliseconds 

---

### `bool addPositionKeyframe(uint16_t time, Point position, EaseType easing=EaseType::LINEAR)`

Add position keyframe. 

timeTime in milliseconds positionPosition at keyframe easingEasing function to next keyframe True if keyframe was added 

---

### `bool addScaleKeyframe(uint16_t time, float scale, EaseType easing=EaseType::LINEAR)`

Add scale keyframe. 

timeTime in milliseconds scaleScale factor at keyframe easingEasing function to next keyframe True if keyframe was added 

---

### `bool addRotationKeyframe(uint16_t time, float rotation, EaseType easing=EaseType::LINEAR)`

Add rotation keyframe. 

timeTime in milliseconds rotationRotation in radians at keyframe easingEasing function to next keyframe True if keyframe was added 

---

### `bool addColorKeyframe(uint16_t time, Pixel4 color, EaseType easing=EaseType::LINEAR)`

Add color keyframe. 

timeTime in milliseconds colorColor at keyframe easingEasing function to next keyframe True if keyframe was added 

---

### `bool createOrbitAnimation(Point center, float radius, uint16_t duration, float startAngle=0.0f, bool clockwise=true)`

Create orbital animation around a center point. 

centerCenter of orbit radiusOrbit radius durationAnimation duration in milliseconds startAngleStarting angle in radians clockwiseTrue for clockwise rotation True if animation was created 

---

### `bool createPulseAnimation(float minScale, float maxScale, uint16_t duration)`

Create pulsing scale animation. 

minScaleMinimum scale factor maxScaleMaximum scale factor durationPulse duration in milliseconds True if animation was created 

---

### `bool createFadeAnimation(Pixel4 fromColor, Pixel4 toColor, uint16_t duration, bool pingPong=false)`

Create color fade animation. 

fromColorStarting color toColorEnding color durationAnimation duration in milliseconds pingPongTrue to fade back and forth True if animation was created 

---

### `void playAll()`

Play all animations. 


        

---

### `void pauseAll()`

Pause all animations. 


        

---

### `void stopAll()`

Stop all animations. 


        

---

### ` &PositionTrack getPositionTrack()`

Get position track. 

Reference to position animation track 

---

### ` &FloatTrack getScaleTrack()`

Get scale track. 

Reference to scale animation track 

---

### ` &FloatTrack getRotationTrack()`

Get rotation track. 

Reference to rotation animation track 

---

### ` &ColorTrack getColorTrack()`

Get color track. 

Reference to color animation track 

---

### `void setAutoStart(bool autoStart)`

Set auto-start behavior. 

autoStartTrue to auto-start animations on component start 

---

### `void setUpdatePosition(bool update)`

Enable/disable position updates. 

updateTrue to update position component from animation 

---

### `void setUpdateColor(bool update)`

Enable/disable color updates. 

updateTrue to update drawable color from animation 

---

