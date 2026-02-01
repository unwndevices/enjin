---
id: AnimationSystem
title: AnimationSystem
sidebar_label: AnimationSystem
---

# AnimationSystem

Animation system for updating time-based animations. 


Updates all entities with , handling timing, looping, and ping-pong behavior. AnimationComponentstructenjin2_1_1AnimationComponentcompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/systems.hpp`

## Public Methods

### ``virtual void update(float deltaTime) override``

Update all animations. 

paramdeltaTimeTime since last update in seconds 

---

### ``virtual int getPriority() const override const``

Get system priority (animations should run early). 

returnPriority value 

---

## Private Methods

### ``void updateAnimation(AnimationComponent &animation, float deltaTime)``

Update individual animation component. 

paramanimationAnimation to update deltaTimeDelta time 

---

