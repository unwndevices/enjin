---
id: SignalConnection
title: SignalConnection
sidebar_label: SignalConnection
---

# SignalConnection

Auto-disconnecting signal connection. 


RAII wrapper that automatically disconnects when destroyed. 

---

**Namespace:** enjin2

**Header:** include/enjin2/core/signal.hpp

## Public Methods

### `cpp
* SignalConnection(Signal&lt; Args... &gt; *sig, std::function&lt; void(Args...)&gt; callback)*
``

Constructor. 

paramsig to connect to Signalclassenjin2_1_1SignalcompoundcallbackCallback function 

---

### `cpp
* SignalConnection(SignalConnection &&other) noexcept*
``

Move constructor. 


        

---

### `cpp
* &SignalConnectionclassenjin2_1_1SignalConnection_1accd0db24b7bbb5ff69ec86b2a5c846bamember operator=(SignalConnection &&other) noexcept*
``

Move assignment. 


        

---

### `cpp
* ~SignalConnection()*
``

Destructor - automatically disconnects. 


        

---

### `cpp
*void disconnect()*
``

Manually disconnect. 


        

---

### `cpp
*bool isConnected() const const*
``

Check if connection is valid. 

returnTrue if connected 

---

### `cpp
* SignalConnection(const SignalConnection &)=delete*
``


        


        

---

### `cpp
* &SignalConnectionclassenjin2_1_1SignalConnection_1accd0db24b7bbb5ff69ec86b2a5c846bamember operator=(const SignalConnection &)=delete*
``


        


        

---

