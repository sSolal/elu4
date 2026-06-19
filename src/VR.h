#pragma once
// OpenXR integration for the 4D game (Route A — PCVR via WiVRn/Monado).
//
// Compiled only when the build is configured with -DBUILD_VR=ON, which defines
// HAVE_OPENXR. Without it this header is an empty shell, so the plain desktop
// build never sees OpenXR. main.cpp guards every use behind HAVE_OPENXR too.
#ifdef HAVE_OPENXR

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct GLFWwindow;

// One eye's render target for the current frame. `framebuffer` is a GL FBO with
// the eye's swapchain colour texture + a depth attachment already bound; render
// into it at (0,0,width,height). `view`/`proj` are that eye's matrices in the
// runtime's LOCAL reference space (origin at the head pose at session start).
struct VREye {
    glm::mat4    view;
    glm::mat4    proj;
    unsigned int framebuffer;
    int          width;
    int          height;
};

// Thin wrapper over the OpenXR session lifecycle + per-frame loop. Binds to the
// GLFW window's existing GLX/OpenGL context, so all the game's GL rendering code
// works unchanged — we only swap where the matrices come from and what FBO the
// scene is drawn into.
class VRSystem {
public:
    // Create instance/system/session/swapchains bound to `window`'s GL context.
    // Returns false (and logs) if no runtime/headset is available — caller then
    // falls back to the desktop path.
    bool init(GLFWwindow* window);
    void shutdown();

    bool isActive() const { return active_; }

    // Pump the OpenXR event queue: drives the session state machine
    // (READY→begin, STOPPING→end) and sets running()/sessionRunning_.
    void pollEvents();
    // False once the runtime asks the app to exit (user quit from the headset).
    bool running() const { return running_; }

    // Start a frame. Returns true and fills `eyes` (one per view, ready to draw)
    // when the runtime wants visible frames this cycle; false when the frame is
    // not rendered (still call endFrame() to submit an empty frame).
    bool beginFrame(std::vector<VREye>& eyes);
    // Release swapchain images and submit the composited projection layer.
    void endFrame();

    // Where the unit scene-cube is placed in the VR room: a comfortable size a
    // little in front of the player at session-start head height. Multiply onto
    // the eye view to get the model→view transform.
    glm::mat4 roomTransform() const;

private:
    struct Impl;            // hides all OpenXR/X11 types from the rest of the app
    Impl* p_ = nullptr;     // pimpl: VR.cpp owns the heavyweight platform headers
    bool  active_  = false; // true once init() fully succeeds
    bool  running_ = true;  // false once the runtime requests app exit
};

#endif // HAVE_OPENXR
