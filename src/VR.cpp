#include "VR.h"
#ifdef HAVE_OPENXR

// GLEW must precede any GL header. GLFW native accessors need the EXPOSE_*
// defines (set as compile definitions in CMake) before glfw3native.h.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GLFW/glfw3native.h>

#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <cstring>
#include <vector>

// Log + bail helpers. XR_CHECK returns false from the caller on failure.
static bool xrFailed(XrResult r, const char* what) {
    if (XR_SUCCEEDED(r)) return false;
    std::fprintf(stderr, "[VR] %s failed (XrResult=%d)\n", what, (int)r);
    return true;
}
#define XR_CHECK(call, what) do { if (xrFailed((call), (what))) return false; } while (0)

// ---- matrix helpers (standard OpenXR → OpenGL conventions) ----

// Asymmetric perspective from an XrFovf (angles in radians, left/down negative),
// producing an OpenGL clip-space (z in [-1, 1]) column-major matrix.
static glm::mat4 projFromFov(const XrFovf& fov, float n, float f) {
    const float tanL = std::tan(fov.angleLeft),  tanR = std::tan(fov.angleRight);
    const float tanD = std::tan(fov.angleDown),  tanU = std::tan(fov.angleUp);
    const float w = tanR - tanL, h = tanU - tanD;
    glm::mat4 m(0.0f);
    m[0][0] = 2.0f / w;
    m[1][1] = 2.0f / h;
    m[2][0] = (tanR + tanL) / w;
    m[2][1] = (tanU + tanD) / h;
    m[2][2] = -(f + n) / (f - n);
    m[2][3] = -1.0f;
    m[3][2] = -(2.0f * f * n) / (f - n);
    return m;
}

// View matrix = inverse of the eye's world pose (rotation then translation).
static glm::mat4 viewFromPose(const XrPosef& p) {
    glm::quat q(p.orientation.w, p.orientation.x, p.orientation.y, p.orientation.z);
    glm::mat4 rot = glm::mat4_cast(q);
    glm::mat4 trans = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(p.position.x, p.position.y, p.position.z));
    return glm::inverse(trans * rot);
}

// ---- all OpenXR/X11 state, hidden from the rest of the app ----

struct VRSystem::Impl {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession  session  = XR_NULL_HANDLE;
    XrSpace    space    = XR_NULL_HANDLE;
    const XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

    struct Swap {
        XrSwapchain handle = XR_NULL_HANDLE;
        int32_t width = 0, height = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        GLuint fbo = 0;
        GLuint depth = 0;
    };
    std::vector<Swap> swaps;                                  // one per view (2)

    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    std::vector<XrView> views;                                // per-view pose+fov
    std::vector<XrCompositionLayerProjectionView> projViews;  // for xrEndFrame

    XrSessionState state = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning = false;                              // between begin/endSession
    bool didRender = false;                                   // did beginFrame draw?
};

// ---- lifecycle ----

bool VRSystem::init(GLFWwindow* window) {
    p_ = new Impl();
    Impl& s = *p_;

    // 1. Instance with the OpenGL graphics extension.
    const char* exts[] = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };
    XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(ici.applicationInfo.applicationName, "4D Game");
    ici.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = exts;
    XR_CHECK(xrCreateInstance(&ici, &s.instance), "xrCreateInstance");

    // 2. System (head-mounted form factor).
    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_CHECK(xrGetSystem(s.instance, &sgi, &s.systemId), "xrGetSystem");

    // 3. View configuration (stereo → 2 views with recommended resolutions).
    uint32_t viewCount = 0;
    XR_CHECK(xrEnumerateViewConfigurationViews(s.instance, s.systemId, s.viewType,
                                               0, &viewCount, nullptr),
             "xrEnumerateViewConfigurationViews(count)");
    std::vector<XrViewConfigurationView> cfgViews(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    XR_CHECK(xrEnumerateViewConfigurationViews(s.instance, s.systemId, s.viewType,
                                               viewCount, &viewCount, cfgViews.data()),
             "xrEnumerateViewConfigurationViews");
    s.views.assign(viewCount, {XR_TYPE_VIEW});
    s.projViews.assign(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

    // 4. Graphics requirements MUST be queried before session creation.
    PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGLReq = nullptr;
    XR_CHECK(xrGetInstanceProcAddr(s.instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                   (PFN_xrVoidFunction*)&pfnGLReq),
             "xrGetInstanceProcAddr(GraphicsRequirements)");
    XrGraphicsRequirementsOpenGLKHR glReq{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    XR_CHECK(pfnGLReq(s.instance, s.systemId, &glReq), "xrGetOpenGLGraphicsRequirements");

    // 5. GLX graphics binding from the GLFW window's existing context. GLFW does
    //    not expose the GLXFBConfig/visualid, so recover them from the context's
    //    GLX_FBCONFIG_ID (the standard workaround).
    Display*    xDisplay = glfwGetX11Display();
    GLXContext  glxCtx   = glfwGetGLXContext(window);
    GLXDrawable glxDraw  = glfwGetGLXWindow(window);
    int fbcId = 0;
    glXQueryContext(xDisplay, glxCtx, GLX_FBCONFIG_ID, &fbcId);
    int nCfg = 0;
    GLXFBConfig* cfgs = glXGetFBConfigs(xDisplay, DefaultScreen(xDisplay), &nCfg);
    GLXFBConfig  chosen = nullptr;
    for (int i = 0; i < nCfg; ++i) {
        int id = 0;
        glXGetFBConfigAttrib(xDisplay, cfgs[i], GLX_FBCONFIG_ID, &id);
        if (id == fbcId) { chosen = cfgs[i]; break; }
    }
    uint32_t visualId = 0;
    if (chosen) {
        if (XVisualInfo* vi = glXGetVisualFromFBConfig(xDisplay, chosen)) {
            visualId = (uint32_t)vi->visualid;
            XFree(vi);
        }
    }
    if (cfgs) XFree(cfgs);

    XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    binding.xDisplay    = xDisplay;
    binding.visualid    = visualId;
    binding.glxFBConfig = chosen;
    binding.glxDrawable = glxDraw;
    binding.glxContext  = glxCtx;

    // 6. Session.
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = s.systemId;
    XR_CHECK(xrCreateSession(s.instance, &sci, &s.session), "xrCreateSession");

    // 7. LOCAL reference space (origin at the head pose at session start).
    XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation.w = 1.0f;
    XR_CHECK(xrCreateReferenceSpace(s.session, &rsci, &s.space), "xrCreateReferenceSpace");

    // 8. Pick a colour swapchain format (prefer 8-bit RGBA, sRGB or linear).
    uint32_t fmtCount = 0;
    XR_CHECK(xrEnumerateSwapchainFormats(s.session, 0, &fmtCount, nullptr),
             "xrEnumerateSwapchainFormats(count)");
    std::vector<int64_t> formats(fmtCount);
    XR_CHECK(xrEnumerateSwapchainFormats(s.session, fmtCount, &fmtCount, formats.data()),
             "xrEnumerateSwapchainFormats");
    int64_t colorFormat = formats.empty() ? GL_SRGB8_ALPHA8 : formats[0];
    for (int64_t f : formats)
        if (f == GL_SRGB8_ALPHA8) { colorFormat = f; break; }

    // 9. One swapchain per view, plus a per-view FBO and depth renderbuffer.
    s.swaps.resize(viewCount);
    for (uint32_t i = 0; i < viewCount; ++i) {
        Impl::Swap& sc = s.swaps[i];
        sc.width  = (int32_t)cfgViews[i].recommendedImageRectWidth;
        sc.height = (int32_t)cfgViews[i].recommendedImageRectHeight;

        XrSwapchainCreateInfo scci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        scci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        scci.format = colorFormat;
        scci.sampleCount = 1;
        scci.width = sc.width;
        scci.height = sc.height;
        scci.faceCount = 1;
        scci.arraySize = 1;
        scci.mipCount = 1;
        XR_CHECK(xrCreateSwapchain(s.session, &scci, &sc.handle), "xrCreateSwapchain");

        uint32_t imgCount = 0;
        XR_CHECK(xrEnumerateSwapchainImages(sc.handle, 0, &imgCount, nullptr),
                 "xrEnumerateSwapchainImages(count)");
        sc.images.assign(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        XR_CHECK(xrEnumerateSwapchainImages(sc.handle, imgCount, &imgCount,
                     (XrSwapchainImageBaseHeader*)sc.images.data()),
                 "xrEnumerateSwapchainImages");

        glGenFramebuffers(1, &sc.fbo);
        glGenRenderbuffers(1, &sc.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, sc.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, sc.width, sc.height);
        glBindFramebuffer(GL_FRAMEBUFFER, sc.fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, sc.depth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    active_ = true;
    std::fprintf(stderr, "[VR] OpenXR session ready: %u views, %dx%d/eye\n",
                 viewCount, s.swaps[0].width, s.swaps[0].height);
    return true;
}

void VRSystem::pollEvents() {
    if (!p_) return;
    Impl& s = *p_;
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(s.instance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* e = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            s.state = e->state;
            if (s.state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = s.viewType;
                if (XR_SUCCEEDED(xrBeginSession(s.session, &bi))) s.sessionRunning = true;
            } else if (s.state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(s.session);
                s.sessionRunning = false;
            } else if (s.state == XR_SESSION_STATE_EXITING ||
                       s.state == XR_SESSION_STATE_LOSS_PENDING) {
                running_ = false;
            }
        }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool VRSystem::beginFrame(std::vector<VREye>& eyes) {
    eyes.clear();
    if (!p_) return false;
    Impl& s = *p_;
    s.didRender = false;
    if (!s.sessionRunning) return false;          // not READY yet — skip cleanly

    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
    s.frameState = XrFrameState{XR_TYPE_FRAME_STATE};
    if (xrFailed(xrWaitFrame(s.session, &wi, &s.frameState), "xrWaitFrame")) return false;

    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    if (xrFailed(xrBeginFrame(s.session, &bi), "xrBeginFrame")) return false;

    if (!s.frameState.shouldRender) return false; // runtime says don't draw

    // Locate the per-eye poses/fovs for this frame's predicted display time.
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
    li.viewConfigurationType = s.viewType;
    li.displayTime = s.frameState.predictedDisplayTime;
    li.space = s.space;
    XrViewState vs{XR_TYPE_VIEW_STATE};
    uint32_t n = (uint32_t)s.views.size();
    if (xrFailed(xrLocateViews(s.session, &li, &vs, n, &n, s.views.data()),
                 "xrLocateViews")) return false;

    for (uint32_t i = 0; i < n; ++i) {
        Impl::Swap& sc = s.swaps[i];
        uint32_t idx = 0;
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (xrFailed(xrAcquireSwapchainImage(sc.handle, &ai, &idx), "xrAcquireSwapchainImage"))
            return false;
        XrSwapchainImageWaitInfo swi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        swi.timeout = XR_INFINITE_DURATION;
        if (xrFailed(xrWaitSwapchainImage(sc.handle, &swi), "xrWaitSwapchainImage"))
            return false;

        // Attach this acquired colour image to the eye's FBO for the caller.
        glBindFramebuffer(GL_FRAMEBUFFER, sc.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               sc.images[idx].image, 0);

        s.projViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        s.projViews[i].pose = s.views[i].pose;
        s.projViews[i].fov  = s.views[i].fov;
        s.projViews[i].subImage.swapchain = sc.handle;
        s.projViews[i].subImage.imageRect.offset = {0, 0};
        s.projViews[i].subImage.imageRect.extent = {sc.width, sc.height};
        s.projViews[i].subImage.imageArrayIndex = 0;

        VREye e;
        e.view = viewFromPose(s.views[i].pose);
        e.proj = projFromFov(s.views[i].fov, 0.05f, 200.0f);
        e.framebuffer = sc.fbo;
        e.width = sc.width;
        e.height = sc.height;
        eyes.push_back(e);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    s.didRender = true;
    return true;
}

void VRSystem::endFrame() {
    if (!p_) return;
    Impl& s = *p_;
    if (!s.sessionRunning) return;

    if (s.didRender) {
        for (auto& sc : s.swaps) {
            XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(sc.handle, &ri);
        }
    }

    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = s.space;
    layer.viewCount = (uint32_t)s.projViews.size();
    layer.views = s.projViews.data();
    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)
    };

    XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
    ei.displayTime = s.frameState.predictedDisplayTime;
    ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    ei.layerCount = s.didRender ? 1 : 0;
    ei.layers = s.didRender ? layers : nullptr;
    xrEndFrame(s.session, &ei);
}

glm::mat4 VRSystem::roomTransform() const {
    // Place the unit scene-cube ~1.5 m in front at head height, a bit under
    // 1 m across — close enough to lean into, small enough to take in at once.
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.5f));
    return glm::scale(m, glm::vec3(0.8f));
}

void VRSystem::shutdown() {
    if (!p_) return;
    Impl& s = *p_;
    for (auto& sc : s.swaps) {
        if (sc.fbo) glDeleteFramebuffers(1, &sc.fbo);
        if (sc.depth) glDeleteRenderbuffers(1, &sc.depth);
        if (sc.handle) xrDestroySwapchain(sc.handle);
    }
    if (s.space) xrDestroySpace(s.space);
    if (s.session) xrDestroySession(s.session);
    if (s.instance) xrDestroyInstance(s.instance);
    delete p_;
    p_ = nullptr;
    active_ = false;
}

#endif // HAVE_OPENXR
