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

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object 

---

### `virtual  ~C_Animation()=default`

Destructor. 


        

---

### `virtual void start() override`

 start - set up animation connections. Componentclassenjin2_1_1Componentcompound


        

---

### `virtual void update(uint16_t deltaTime) override`

 update - update all active tracks. Componentclassenjin2_1_1Componentcompound

paramdeltaTimeTime elapsed since last update in milliseconds 

---

### `bool addPositionKeyframe(uint16_t time, Point position, EaseType easing=EaseType::LINEAR)`

Add position keyframe. 

paramtimeTime in milliseconds positionPosition at keyframe easingEasing function to next keyframe returnTrue if keyframe was added 

---

### `bool addScaleKeyframe(uint16_t time, float scale, EaseType easing=EaseType::LINEAR)`

Add scale keyframe. 

paramtimeTime in milliseconds scaleScale factor at keyframe easingEasing function to next keyframe returnTrue if keyframe was added 

---

### `bool addRotationKeyframe(uint16_t time, float rotation, EaseType easing=EaseType::LINEAR)`

Add rotation keyframe. 

paramtimeTime in milliseconds rotationRotation in radians at keyframe easingEasing function to next keyframe returnTrue if keyframe was added 

---

### `bool addColorKeyframe(uint16_t time, Pixel4 color, EaseType easing=EaseType::LINEAR)`

Add color keyframe. 

paramtimeTime in milliseconds colorColor at keyframe easingEasing function to next keyframe returnTrue if keyframe was added 

---

### `bool createOrbitAnimation(Point center, float radius, uint16_t duration, float startAngle=0.0f, bool clockwise=true)`

Create orbital animation around a center point. 

paramcenterCenter of orbit radiusOrbit radius durationAnimation duration in milliseconds startAngleStarting angle in radians clockwiseTrue for clockwise rotation returnTrue if animation was created 

---

### `bool createPulseAnimation(float minScale, float maxScale, uint16_t duration)`

Create pulsing scale animation. 

paramminScaleMinimum scale factor maxScaleMaximum scale factor durationPulse duration in milliseconds returnTrue if animation was created 

---

### `bool createFadeAnimation(Pixel4 fromColor, Pixel4 toColor, uint16_t duration, bool pingPong=false)`

Create color fade animation. 

paramfromColorStarting color toColorEnding color durationAnimation duration in milliseconds pingPongTrue to fade back and forth returnTrue if animation was created 

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

### ` &PositionTrackanimation__track_8hpp_1a705bce4c4d55a7271e6efce902ed58a7member getPositionTrack()`

Get position track. 

returnReference to position animation track 

---

### ` &FloatTrackanimation__track_8hpp_1a81f4d3d5d4fc690b738247c59d607058member getScaleTrack()`

Get scale track. 

returnReference to scale animation track 

---

### ` &FloatTrackanimation__track_8hpp_1a81f4d3d5d4fc690b738247c59d607058member getRotationTrack()`

Get rotation track. 

returnReference to rotation animation track 

---

### ` &ColorTrackanimation__track_8hpp_1a475c8c4e4bdf2545136e90a1955ba0abmember getColorTrack()`

Get color track. 

returnReference to color animation track 

---

### `void setAutoStart(bool autoStart)`

Set auto-start behavior. 

paramautoStartTrue to auto-start animations on component start 

---

### `void setUpdatePosition(bool update)`

Enable/disable position updates. 

paramupdateclassenjin2_1_1C__Animation_1af045b100ac6c4d25bec1f4bafcb90aa7memberTrue to update position component from animation 

---

### `void setUpdateColor(bool update)`

Enable/disable color updates. 

paramupdateclassenjin2_1_1C__Animation_1af045b100ac6c4d25bec1f4bafcb90aa7memberTrue to update drawable color from animation 

---

