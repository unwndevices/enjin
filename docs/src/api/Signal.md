---
id: Signal
title: Signal
sidebar_label: Signal
---

# Signal

 class for observer pattern implementation. Signalclassenjin2_1_1Signalcompound


Provides a lightweight event system using static allocation.  can be emitted with parameters and connected to callbacks. Signalsnamespaceenjin2_1_1Signalscompound

---

**Namespace:** enjin2

**Header:** include/enjin2/core/signal.hpp

## Public Methods

### `cpp
* Signal()*
``

Constructor. 


        

---

### `cpp
*int connect(CallbackType callback)*
``

Connect a callback to this signal. 

paramcallbackFunction to call when signal is emitted returnConnection ID (can be used to disconnect) or -1 if failed 

---

### `cpp
*void disconnect(int connectionId)*
``

Disconnect a callback by connection ID. 

paramconnectionIdConnection ID returned by  connect()classenjin2_1_1Signal_1a3842d5435fcb3bef84a5eb16531e7aabmember

---

### `cpp
*void emit(Args... args)*
``

Emit the signal, calling all connected callbacks. 

paramargsArguments to pass to callbacks 

---

### `cpp
*void disconnectAll()*
``

Disconnect all callbacks. 


        

---

### `cpp
*size_t getConnectionCount() const const*
``

Get number of active connections. 

returnNumber of connected callbacks 

---

### `cpp
*bool hasConnections() const const*
``

Check if signal has any connections. 

returnTrue if there are active connections 

---

### `cpp
*void operator()(Args... args)*
``

Operator() for emitting signals. 

paramargsArguments to pass to callbacks 

---

