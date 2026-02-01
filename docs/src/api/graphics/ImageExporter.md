---
id: ImageExporter
title: ImageExporter
sidebar_label: ImageExporter
---

# ImageExporter

Simple image export utilities for visualizing canvas output. 


Provides basic image export functionality to visualize the canvas content as actual images rather than ASCII output. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/image_export.hpp`

## Public Methods

### ``static bool exportToPGM(const Canvas4&lt; WIDTH, HEIGHT &gt; &canvas, const std::string &filename, int scale=4)``

Export 4-bit canvas to PGM (Portable Graymap) format. 

paramcanvasCanvas to export filenameOutput filename scaleScale factor for output image (1 = 1:1, 2 = 2x size, etc.) returnTrue if export succeeded 

---

### ``static bool exportToPGM(const Canvas8&lt; WIDTH, HEIGHT &gt; &canvas, const std::string &filename, int scale=4)``

Export 8-bit canvas to PGM format. 

paramcanvasCanvas to export filenameOutput filename scaleScale factor for output image returnTrue if export succeeded 

---

### ``static void printVisual(const Canvas4&lt; WIDTH, HEIGHT &gt; &canvas, const std::string &title="")``

Export 4-bit canvas to simple visual ASCII format (better than before). 

paramcanvasCanvas to export titleTitle to display above the visualization 

---

### ``static void printColorVisual(const Canvas4&lt; WIDTH, HEIGHT &gt; &canvas, const std::string &title="")``

Create a color-coded terminal visualization using ANSI colors. 

paramcanvasCanvas to display titleTitle to display 

---

