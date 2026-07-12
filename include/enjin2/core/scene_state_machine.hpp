#pragma once

#include "scene.hpp"
#include "signal.hpp"
#include <array>
#include <memory>

namespace enjin2 {

/**
 * @brief State machine for managing scene transitions
 * 
 * Provides a centralized system for managing multiple scenes,
 * handling transitions between them, and maintaining scene state.
 */
class SceneStateMachine {
public:
    /**
     * @brief Scene transition types
     */
    enum class TransitionType {
        IMMEDIATE,      ///< Instant transition
        FADE_OUT_IN,    ///< Fade out current, fade in new
        SLIDE_LEFT,     ///< Slide transition to the left
        SLIDE_RIGHT,    ///< Slide transition to the right
        SLIDE_UP,       ///< Slide transition up
        SLIDE_DOWN      ///< Slide transition down
    };
    
    /**
     * @brief Transition state
     */
    enum class TransitionState {
        IDLE,           ///< No transition active
        FADING_OUT,     ///< Fading out current scene
        FADING_IN,      ///< Fading in new scene
        SLIDING         ///< Sliding between scenes
    };

public:
    /**
     * @brief Fixed-size registry for persistent objects that survive scene transitions.
     *
     * Owned by SceneStateMachine. Stores unique_ptr ownership of extracted objects.
     * Objects are injected into scene ObjectCollections as non-owning externals.
     */
    struct PersistentObjectRegistry {
        static constexpr int MAX_PERSISTENT = 4;  ///< Maximum number of persistent objects

        std::unique_ptr<Object> objects[MAX_PERSISTENT];  ///< Owned persistent objects (nullptr = free slot)
        bool pendingRemoval[MAX_PERSISTENT]{};            ///< Slots flagged for removal at the next transition

        /**
         * @brief Add an object to the registry
         * @param obj unique_ptr to add (ownership transferred)
         * @return true if added, false if registry is full
         */
        bool add(std::unique_ptr<Object> obj) {
            for (int i = 0; i < MAX_PERSISTENT; ++i) {
                if (!objects[i]) {
                    objects[i] = std::move(obj);
                    pendingRemoval[i] = false;
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Mark an object for removal on next transition
         * @param obj Raw pointer of the object to mark
         */
        void markForRemoval(Object* obj) {
            for (int i = 0; i < MAX_PERSISTENT; ++i) {
                if (objects[i].get() == obj) {
                    pendingRemoval[i] = true;
                    return;
                }
            }
        }

        /**
         * @brief Check if an object is already in the registry
         * @param obj Raw pointer to check
         * @return true if found (ignores pendingRemoval state)
         */
        bool contains(Object* obj) const {
            for (int i = 0; i < MAX_PERSISTENT; ++i) {
                if (objects[i].get() == obj) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Find a non-removed object by name
         * @param name Name to search for
         * @return Pointer to matching Object or nullptr
         */
        Object* findByName(const char* name) const {
            if (!name) return nullptr;
            for (int i = 0; i < MAX_PERSISTENT; ++i) {
                if (objects[i] && !pendingRemoval[i] &&
                    objects[i]->getName() &&
                    strcmp(objects[i]->getName(), name) == 0) {
                    return objects[i].get();
                }
            }
            return nullptr;
        }

        /**
         * @brief Destroy all objects marked for removal and clear their slots
         *
         * Resetting the unique_ptr destroys the Object, which sets
         * ObjectProxy::valid = false in ~Object().
         */
        void flushPendingRemovals() {
            for (int i = 0; i < MAX_PERSISTENT; ++i) {
                if (pendingRemoval[i]) {
                    objects[i].reset();
                    pendingRemoval[i] = false;
                }
            }
        }
    };

private:
    static constexpr size_t MAX_SCENES = 32;        ///< Maximum number of scenes
    static constexpr float TRANSITION_TIME = 0.5f;  ///< Default transition time in seconds

    std::array<std::unique_ptr<Scene>, MAX_SCENES> scenes;
    size_t sceneCount;
    Scene* currentScene;
    Scene* nextScene;

    PersistentObjectRegistry m_persistentRegistry;  ///< Registry for persistent objects
    
    TransitionState transitionState;
    TransitionType transitionType;
    float transitionTimer;
    float transitionDuration;
    
    // Transition progress (0.0 to 1.0)
    float transitionProgress;

    uint32_t pendingSceneId = 0;       ///< ID of the pending deferred transition target
    bool hasPendingTransition = false;  ///< True when a deferred transition is queued

    // Signals for scene transition events
    Signal<Scene*, Scene*> onSceneChangeStartSignal;    ///< (from, to)
    Signal<Scene*, Scene*> onSceneChangeCompleteSignal; ///< (from, to)
    Signal<TransitionType> onTransitionStartSignal;     ///< Transition type
    Signal<float> onTransitionProgressSignal;           ///< Progress (0.0-1.0)
    
public:
    /**
     * @brief Constructor
     */
    SceneStateMachine() 
        : sceneCount(0), currentScene(nullptr), nextScene(nullptr),
          transitionState(TransitionState::IDLE), transitionType(TransitionType::IMMEDIATE),
          transitionTimer(0.0f), transitionDuration(TRANSITION_TIME), transitionProgress(0.0f) {
        for (auto& scene : scenes) {
            scene = nullptr;
        }
    }
    
    /**
     * @brief Destructor
     */
    ~SceneStateMachine() = default;
    
    /**
     * @brief Add a scene to the state machine
     * @tparam T Scene type (must derive from Scene)
     * @tparam Args Constructor argument types
     * @param sceneId Unique scene identifier
     * @param args Constructor arguments
     * @return Pointer to created scene or nullptr if failed
     */
    template<typename T, typename... Args>
    T* addScene(uint32_t sceneId, Args&&... args) {
        static_assert(std::is_base_of<Scene, T>::value, "T must derive from Scene");
        
        if (sceneCount >= MAX_SCENES) {
            return nullptr;
        }
        
        // Check if scene ID already exists
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == sceneId) {
                return nullptr; // Scene ID already exists
            }
        }
        
        std::unique_ptr<T> scene(new T(sceneId, std::forward<Args>(args)...));
        T* scenePtr = scene.get();
        scenes[sceneCount++] = std::move(scene);
        
        return scenePtr;
    }
    
    /**
     * @brief Remove a scene from the state machine
     * @param sceneId Scene identifier to remove
     * @return True if scene was removed
     */
    bool removeScene(uint32_t sceneId) {
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == sceneId) {
                // Don't remove if it's the current scene
                if (scenes[i].get() == currentScene) {
                    return false;
                }
                
                // Shift remaining scenes
                for (size_t j = i; j < sceneCount - 1; ++j) {
                    scenes[j] = std::move(scenes[j + 1]);
                }
                sceneCount--;
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Change to a different scene
     * @param sceneId Target scene identifier
     * @param transition Transition type to use
     * @param duration Transition duration in seconds (0 = use default)
     * @return True if transition started successfully
     */
    bool changeScene(uint32_t sceneId, TransitionType transition = TransitionType::IMMEDIATE,
                     float duration = 0.0f) {
        // Find target scene
        Scene* targetScene = nullptr;
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == sceneId) {
                targetScene = scenes[i].get();
                break;
            }
        }
        
        if (!targetScene) {
            return false; // Scene not found
        }
        
        if (targetScene == currentScene) {
            return true; // Already current scene
        }
        
        // If already transitioning, abort
        if (transitionState != TransitionState::IDLE) {
            return false;
        }
        
        nextScene = targetScene;
        transitionType = transition;
        transitionDuration = duration > 0.0f ? duration : TRANSITION_TIME;
        transitionTimer = 0;
        transitionProgress = 0.0f;
        
        // Emit transition start signal
        onSceneChangeStartSignal.emit(currentScene, nextScene);
        onTransitionStartSignal.emit(transition);
        
        if (transition == TransitionType::IMMEDIATE) {
            completeTransition();
        } else {
            startTransition();
        }
        
        return true;
    }
    
    /**
     * @brief Request a deferred scene transition (safe to call from onUpdate())
     *
     * Unlike changeScene(), switchTo() queues the transition for execution AFTER the
     * current scene's update() returns, preventing re-entrant corruption. Calling
     * switchTo(currentId) triggers a full reset: onDeactivate, onCreate, onActivate.
     *
     * Last-wins: multiple switchTo() calls in the same frame overwrite each other.
     * Re-entrancy from onDeactivate(): safely queued for the next frame.
     *
     * @param sceneId Target scene identifier (must already be registered via addScene)
     */
    void switchTo(uint32_t sceneId) {
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == sceneId) {
                pendingSceneId = sceneId;
                hasPendingTransition = true;
                return;
            }
        }
        // Unknown scene ID: silently ignored (embedded release constraint)
    }

    /**
     * @brief Update the state machine
     * @param dt Time since last frame in seconds
     */
    void update(float dt) {
        // Update transition if active
        if (transitionState != TransitionState::IDLE) {
            updateTransition(dt);
        }

        // Update current scene
        if (currentScene) {
            currentScene->update(dt);
        }

        // Deferred transition: executes AFTER currentScene->update() returns (no re-entrancy)
        // hasPendingTransition MUST be cleared before applyDeferredTransition() so that
        // any switchTo() called from onDeactivate() queues for the NEXT frame, not this one.
        if (hasPendingTransition) {
            hasPendingTransition = false;
            uint32_t targetId = pendingSceneId;
            pendingSceneId = 0;
            applyDeferredTransition(targetId);
        }
    }
    
    /**
     * @brief Render the current scene with transition effects
     * @param canvas Target canvas for rendering
     */
    template<typename PixelType>
    void render(ICanvas<PixelType>& canvas) {
        if (transitionState == TransitionState::IDLE) {
            // Normal rendering
            if (currentScene) {
                currentScene->render(canvas);
            }
        } else {
            // Render with transition effects
            renderWithTransition(canvas);
        }
    }
    
    /**
     * @brief Get current scene
     * @return Pointer to current scene or nullptr
     */
    Scene* getCurrentScene() {
        return currentScene;
    }
    
    /**
     * @brief Get scene by ID
     * @param sceneId Scene identifier
     * @return Pointer to scene or nullptr if not found
     */
    Scene* getScene(uint32_t sceneId) {
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == sceneId) {
                return scenes[i].get();
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Check if a transition is currently active
     * @return True if transitioning
     */
    bool isTransitioning() const {
        return transitionState != TransitionState::IDLE;
    }
    
    /**
     * @brief Get current transition progress
     * @return Progress value from 0.0 to 1.0
     */
    float getTransitionProgress() const {
        return transitionProgress;
    }
    
    /**
     * @brief Connect to scene change start event
     * @param callback Function called with (from_scene, to_scene)
     * @return Signal connection handle
     */
    SignalConnection<Scene*, Scene*> connectOnSceneChangeStart(std::function<void(Scene*, Scene*)> callback) {
        return SignalConnection<Scene*, Scene*>(&onSceneChangeStartSignal, callback);
    }

    /**
     * @brief Connect to scene change complete event
     * @param callback Function called with (from_scene, to_scene)
     * @return Signal connection handle
     */
    SignalConnection<Scene*, Scene*> connectOnSceneChangeComplete(std::function<void(Scene*, Scene*)> callback) {
        return SignalConnection<Scene*, Scene*>(&onSceneChangeCompleteSignal, callback);
    }

    /**
     * @brief Extract an object from the current scene and register it as persistent.
     *
     * The object survives all subsequent scene transitions. It is immediately
     * re-injected as a non-owning external into the current scene's ObjectCollection,
     * so it continues to participate in update/lateUpdate/findByName/forEach during
     * the current scene and all subsequent scenes. If the object is already persistent,
     * returns true (silent no-op — no duplicate slot consumed).
     *
     * @param obj Raw pointer to an owned object in the current scene
     * @return true if now persistent, false if object not in current scene or registry full
     */
    bool persistObject(Object* obj) {
        if (!obj || !currentScene) return false;
        if (m_persistentRegistry.contains(obj)) return true;  // already persistent
        std::unique_ptr<Object> extracted = currentScene->getObjects().extractObject(obj);
        if (!extracted) return false;
        Object* rawPtr = extracted.get();
        if (!m_persistentRegistry.add(std::move(extracted))) return false;
        // Immediately inject back into current scene as non-owning external
        currentScene->getObjects().injectExternal(rawPtr);
        return true;
    }

    /**
     * @brief Schedule a persistent object for destruction on the next scene transition.
     *
     * The object remains alive until applyDeferredTransition() calls
     * flushPendingRemovals(). Its proxy will be invalidated at that point.
     *
     * @param obj Raw pointer of the persistent object to remove
     */
    void unpersistObject(Object* obj) {
        m_persistentRegistry.markForRemoval(obj);
    }

    /**
     * @brief Find a persistent object by name (skips pending-removal objects)
     * @param name Name to search for
     * @return Pointer to matching Object or nullptr
     */
    Object* findPersistentByName(const char* name) const {
        return m_persistentRegistry.findByName(name);
    }

    /**
     * @brief Connect to transition start event
     * @param callback Function called with transition type
     * @return Signal connection handle
     */
    SignalConnection<TransitionType> connectOnTransitionStart(std::function<void(TransitionType)> callback) {
        return SignalConnection<TransitionType>(&onTransitionStartSignal, callback);
    }

    /**
     * @brief Connect to transition progress event
     * @param callback Function called with progress value (0.0-1.0)
     * @return Signal connection handle
     */
    SignalConnection<float> connectOnTransitionProgress(std::function<void(float)> callback) {
        return SignalConnection<float>(&onTransitionProgressSignal, callback);
    }

private:
    /**
     * @brief Start a transition
     */
    void startTransition() {
        switch (transitionType) {
            case TransitionType::FADE_OUT_IN:
                transitionState = TransitionState::FADING_OUT;
                break;
                
            case TransitionType::SLIDE_LEFT:
            case TransitionType::SLIDE_RIGHT:
            case TransitionType::SLIDE_UP:
            case TransitionType::SLIDE_DOWN:
                transitionState = TransitionState::SLIDING;
                if (nextScene) {
                    nextScene->setStateMachine(this);  // inject BEFORE initialize() and activate()
                }
                if (nextScene && !nextScene->isInitialized()) {
                    nextScene->initialize();
                }
                if (nextScene && !nextScene->isActive()) {
                    nextScene->activate();
                }
                break;
                
            default:
                completeTransition();
                break;
        }
    }
    
    /**
     * @brief Apply a deferred transition queued by switchTo()
     *
     * Called from update() after currentScene->update() returns.
     * Self-transitions trigger a full reset cycle: deactivate -> resetInitialized -> activate.
     * Cross-scene transitions are equivalent to an immediate changeScene().
     *
     * @param targetId Scene ID to transition to
     */
    void applyDeferredTransition(uint32_t targetId) {
        Scene* targetScene = nullptr;
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == targetId) {
                targetScene = scenes[i].get();
                break;
            }
        }
        if (!targetScene) return;

        bool isSelf = (targetScene == currentScene);
        if (isSelf) {
            // Destroy objects scheduled for removal before clearing externals
            m_persistentRegistry.flushPendingRemovals();
            // Full reset: deactivate -> clear init guard -> re-activate
            Scene* scene = currentScene;
            scene->getObjects().clearExternal();  // detach persistent pointers before reset
            scene->deactivate();              // fires onDeactivate(); sets active = false
            scene->resetInitialized();        // sets initialized = false (bypasses guard)
            scene->setStateMachine(this);     // re-inject (already set; explicit for clarity)
            // Re-inject surviving persistent objects into the reset scene
            for (int i = 0; i < PersistentObjectRegistry::MAX_PERSISTENT; ++i) {
                if (m_persistentRegistry.objects[i]) {
                    scene->getObjects().injectExternal(m_persistentRegistry.objects[i].get());
                }
            }
            scene->activate();               // initialize() fires -> onCreate(); then onActivate()
        } else {
            // Destroy objects scheduled for removal before clearing old scene's externals
            m_persistentRegistry.flushPendingRemovals();
            // Normal cross-scene immediate transition
            Scene* oldScene = currentScene;
            if (currentScene) {
                currentScene->getObjects().clearExternal();  // detach persistent pointers before deactivate
                currentScene->deactivate();
            }
            currentScene = targetScene;
            currentScene->setStateMachine(this);
            // Inject persistent objects into new scene
            for (int i = 0; i < PersistentObjectRegistry::MAX_PERSISTENT; ++i) {
                if (m_persistentRegistry.objects[i]) {
                    currentScene->getObjects().injectExternal(m_persistentRegistry.objects[i].get());
                }
            }
            if (!currentScene->isInitialized()) { currentScene->initialize(); }
            if (!currentScene->isActive()) { currentScene->activate(); }
            onSceneChangeCompleteSignal.emit(oldScene, currentScene);
        }
    }

    /**
     * @brief Update transition state
     * @param dt Time since last frame in seconds
     */
    void updateTransition(float dt) {
        transitionTimer += dt;
        transitionProgress = transitionTimer / transitionDuration;
        
        if (transitionProgress >= 1.0f) {
            transitionProgress = 1.0f;
            completeTransition();
        } else {
            onTransitionProgressSignal.emit(transitionProgress);
            
            // Handle mid-transition state changes
            if (transitionType == TransitionType::FADE_OUT_IN && 
                transitionState == TransitionState::FADING_OUT && 
                transitionProgress >= 0.5f) {
                
                // Switch scenes at halfway point
                if (currentScene) {
                    currentScene->deactivate();
                }
                currentScene = nextScene;
                if (currentScene && !currentScene->isInitialized()) {
                    currentScene->initialize();
                }
                if (currentScene && !currentScene->isActive()) {
                    currentScene->activate();
                }
                transitionState = TransitionState::FADING_IN;
            }
        }
    }
    
    /**
     * @brief Complete the current transition
     */
    void completeTransition() {
        Scene* oldScene = currentScene;
        
        // Deactivate current scene
        if (currentScene && currentScene != nextScene) {
            currentScene->deactivate();
        }
        
        // Activate next scene
        currentScene = nextScene;
        if (currentScene) {
            currentScene->setStateMachine(this);   // inject BEFORE initialize() and activate()
        }
        if (currentScene && !currentScene->isInitialized()) {
            currentScene->initialize();
        }
        if (currentScene && !currentScene->isActive()) {
            currentScene->activate();
        }
        
        // Reset transition state
        transitionState = TransitionState::IDLE;
        transitionProgress = 1.0f;
        nextScene = nullptr;
        
        // Emit completion signal
        onSceneChangeCompleteSignal.emit(oldScene, currentScene);
        onTransitionProgressSignal.emit(1.0f);
    }
    
    /**
     * @brief Render scenes with transition effects
     * @param canvas Target canvas
     */
    template<typename PixelType>
    void renderWithTransition(ICanvas<PixelType>& canvas) {
        switch (transitionType) {
            case TransitionType::FADE_OUT_IN:
                renderFadeTransition(canvas);
                break;
                
            case TransitionType::SLIDE_LEFT:
            case TransitionType::SLIDE_RIGHT:
            case TransitionType::SLIDE_UP:
            case TransitionType::SLIDE_DOWN:
                renderSlideTransition(canvas);
                break;
                
            default:
                if (currentScene) {
                    currentScene->render(canvas);
                }
                break;
        }
    }
    
    /**
     * @brief Render fade transition
     * @param canvas Target canvas
     */
    template<typename PixelType>
    void renderFadeTransition(ICanvas<PixelType>& canvas) {
        if (transitionState == TransitionState::FADING_OUT) {
            // Render current scene with fading
            if (currentScene) {
                currentScene->render(canvas);
                // Apply fade effect - would need canvas support for opacity
            }
        } else if (transitionState == TransitionState::FADING_IN) {
            // Render new scene with fading in
            if (currentScene) {
                currentScene->render(canvas);
                // Apply fade-in effect
            }
        }
    }
    
    /**
     * @brief Render slide transition
     * @param canvas Target canvas
     */
    template<typename PixelType>
    void renderSlideTransition(ICanvas<PixelType>& canvas) {
        // For slide transitions, both scenes are rendered with offsets
        // This would require canvas support for viewport offsets
        
        int16_t offset = static_cast<int16_t>(transitionProgress * canvas.getWidth());
        
        switch (transitionType) {
            case TransitionType::SLIDE_LEFT:
                // Current scene slides left, new scene comes from right
                if (currentScene) {
                    // Render current scene shifted left
                    currentScene->render(canvas);
                }
                if (nextScene) {
                    // Render next scene shifted from right
                    nextScene->render(canvas);
                }
                break;
                
            case TransitionType::SLIDE_RIGHT:
                // Current scene slides right, new scene comes from left
                if (currentScene) {
                    currentScene->render(canvas);
                }
                if (nextScene) {
                    nextScene->render(canvas);
                }
                break;
                
            case TransitionType::SLIDE_UP:
            case TransitionType::SLIDE_DOWN:
                // Similar logic for vertical slides
                if (currentScene) {
                    currentScene->render(canvas);
                }
                if (nextScene) {
                    nextScene->render(canvas);
                }
                break;
                
            default:
                break;
        }
    }
};

} // namespace enjin2