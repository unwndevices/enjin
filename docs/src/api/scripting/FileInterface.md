---
id: FileInterface
title: FileInterface
sidebar_label: FileInterface
---

# FileInterface

Abstract file interface for platform independence. 


Allows ImageCache to work with different file systems (embedded flash, SD card, etc.) 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/image_cache.hpp`

## Public Methods

### ``virtual  ~FileInterface()=default``


        


        

---

### ``bool open()=0``


        


        

---

### ``void close()=0``


        


        

---

### ``size_t read(uint8_t *buffer, size_t length)=0``


        


        

---

### ``bool seek(size_t position)=0``


        


        

---

### ``size_t size() const =0 const``


        


        

---

