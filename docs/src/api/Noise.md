---
id: Noise
title: Noise
sidebar_label: Noise
---

# Noise

 generation utilities. Noisenamespaceenjin2_1_1Noisecompound


Provides Perlin noise and various noise functions for procedural generation and visual effects. Based on original Enjin noise implementation. 

---

**Namespace:** enjin2::Noise

**Header:** include/enjin2/utils/noise.hpp

## Functions

### `cpp
*float grad(int hash, float x, float y)*
``

Gradient function for Perlin noise. 

paramhashHash value xX coordinate yY coordinate returnGradient value 

---

### `cpp
*float pnoise(float x, float y, int px, int py)*
``

2D Perlin noise function 

paramxX coordinate yY coordinate pxPeriod in X direction (for tiling) pyPeriod in Y direction (for tiling) return value in range [-1, 1] Noisenamespaceenjin2_1_1Noisecompound

---

### `cpp
*uint8_t warped_pnoise(float nx, float ny, int px, int py, int seed, float warp_strength=0.5f)*
``

Warped Perlin noise for more organic patterns. 

paramnxX coordinate nyY coordinate pxPeriod in X direction pyPeriod in Y direction seedRandom seed warp_strengthWarping strength (0.0 - 1.0) return value in range [0, 255] Noisenamespaceenjin2_1_1Noisecompound

---

### `cpp
*uint8_t value_noise(float x, float y, uint32_t seed)*
``

Simple value noise. 

paramxX coordinate yY coordinate seedRandom seed return value in range [0, 255] Noisenamespaceenjin2_1_1Noisecompound

---

### `cpp
*uint8_t fbm_noise(float x, float y, int octaves=4, float persistence=0.5f, float lacunarity=2.0f)*
``

Fractal Brownian Motion (FBM) noise. 

paramxX coordinate yY coordinate octavesNumber of octaves persistenceAmplitude multiplier per octave lacunarityFrequency multiplier per octave return value in range [0, 255] Noisenamespaceenjin2_1_1Noisecompound

---

### `cpp
*uint8_t cellular_noise(float x, float y, float scale=10.0f)*
``

Cellular/Worley noise. 

paramxX coordinate yY coordinate scaleGrid scale returnDistance to nearest feature point [0, 255] 

---

### `cpp
*uint8_t to_4bit(uint8_t noise_value)*
``

Convert noise to 4-bit range. 

paramnoise_value8-bit noise value return4-bit noise value (0-15) 

---

### `cpp
*void generate_noise_texture(uint8_t *buffer, int width, int height, float scale, float offset_x, float offset_y, int noise_type=0)*
``

Generate noise texture into buffer. 

parambufferOutput buffer widthTexture width heightTexture height scale scale Noisenamespaceenjin2_1_1Noisecompoundoffset_xX offset for animation offset_yY offset for animation noise_type0=perlin, 1=value, 2=fbm, 3=cellular 

---

