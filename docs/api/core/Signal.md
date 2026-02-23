---
id: Signal
title: Signal
sidebar_label: Signal
---

# Signal

 class for observer pattern implementation. Signal


Provides a lightweight event system using static allocation.  can be emitted with parameters and connected to callbacks. Signals

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/signal.hpp`

## Public Methods

### ` Signal()`

Constructor. 


        

---

### `int connect(CallbackType callback)`

Connect a callback to this signal. 

callbackFunction to call when signal is emitted Connection ID (can be used to disconnect) or -1 if failed 

---

### `void disconnect(int connectionId)`

Disconnect a callback by connection ID. 

connectionIdConnection ID returned by  connect()

---

### `void emit(Args... args)`

Emit the signal, calling all connected callbacks. 

argsArguments to pass to callbacks 

---

### `void disconnectAll()`

Disconnect all callbacks. 


        

---

### `size_t getConnectionCount() const const`

Get number of active connections. 

Number of connected callbacks 

---

### `bool hasConnections() const const`

Check if signal has any connections. 

True if there are active connections 

---

### `void operator()(Args... args)`

Operator() for emitting signals. 

argsArguments to pass to callbacks 

---

