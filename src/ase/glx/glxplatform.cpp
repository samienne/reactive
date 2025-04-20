#include "glxplatform.h"

#include "glxrendercontext.h"
#include "glxcontext.h"
#include "glxwindow.h"

#include "commandbuffer.h"
#include "rendercontext.h"
#include "renderqueue.h"
#include "platform.h"

#include "debug.h"

#include <GL/glx.h>

#include <X11/extensions/sync.h>

#include <tracy/Tracy.hpp>

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <unordered_set>
#include <queue>

#include <cstdlib>

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092

namespace ase
{

Platform makeDefaultPlatform()
{
    return Platform(std::make_shared<GlxPlatform>());
}

typedef GLXContext (*glXCreateContextAttribsArbProc)(Display*, GLXFBConfig,
        GLXContext, Bool, const int*);
static glXCreateContextAttribsArbProc glXCreateContextAttribsARB = 0;

static bool ctxErrorOccurred = false;
static int ctxErrorHandler(Display* /*dpy*/, XErrorEvent* /*ev*/)
{
    ctxErrorOccurred = true;
    return 0;
}

static int doubleBufferAttributes[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    GLX_DOUBLEBUFFER,  True,
    GLX_RED_SIZE,      8,
    GLX_GREEN_SIZE,    8,
    GLX_BLUE_SIZE,     8,
    GLX_DEPTH_SIZE,    24,
#ifdef GL_EXT_framebuffer_sRGB
    GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, True,
#endif
    None
};

int dummyBufferAttributes[] = {
    GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
    GLX_PBUFFER_WIDTH, 1,
    GLX_PBUFFER_HEIGHT, 1,
    GLX_PRESERVED_CONTENTS, False,
    None
};

class GlxPlatformDeferred
{
public:
    GlxPlatformDeferred(GlxPlatform& platform);
    ~GlxPlatformDeferred();

    void destroy();
    void printGlInfo();

    void initGl();
    void deinitGl();

private:
    inline GlxPlatform* q() { return &platform_; }
    inline GlxPlatform const* q() const { return &platform_; }

private:
    friend class GlxPlatform;

    GlxPlatform& platform_;

    //GlxPlatform::Mutex mutex_;
    TracyLockable(GlxPlatform::Mutex, mutex_);

    Atom wmDelete_ = 0;
    Atom wmProtocols_ = 0;
    Atom wmSyncRequest_ = 0;

    Display* dpy_ = nullptr;
    GLXFBConfig* configs_ = nullptr;
    size_t configCount_ = 0;
    GLXDrawable dummyBuffer_ = 0;
    std::unordered_set<GLXContext> glxContexts_;
    std::vector<std::weak_ptr<GlxWindow>> windows_;
    bool gl3Enabled_ = true;
    bool gl4Enabled_ = false;
    bool xsync_ = false;
};

GlxPlatformDeferred::GlxPlatformDeferred(GlxPlatform& platform) :
    platform_(platform)
{
    //DBG("GlxPlatform size: %1 bytes.", sizeof(GlxPlatform));
    //DBG("GlxPlatformDeferred size: %1 bytes.", sizeof(GlxPlatformDeferred));
    //DBG("GlxContext size: %1 bytes.", sizeof(GlxContext));
    //DBG("GlxRenderContext size: %1 bytes.", sizeof(GlxRenderContext));
    //DBG("GlxWindow size: %1 bytes.", sizeof(GlxWindow));
}

GlxPlatformDeferred::~GlxPlatformDeferred()
{
    destroy();
}

void GlxPlatformDeferred::destroy()
{
    //deinitGl();

    if (configs_)
        XFree(configs_);
    configs_ = 0;

    if (dummyBuffer_)
        glXDestroyPbuffer(dpy_, dummyBuffer_);
    dummyBuffer_ = 0;

    if (dpy_)
        XCloseDisplay(dpy_);
    dpy_ = 0;
}

void GlxPlatformDeferred::printGlInfo()
{
    DBG("GlxPlatform: Gl vendor: %1", glGetString(GL_VENDOR));
    DBG("GlxPlatform: Gl renderer: %1", glGetString(GL_RENDERER));
    DBG("GlxPlatform: Gl version: %1", glGetString(GL_VERSION));
    DBG("GlxPlatform: GLSL version: %1", glGetString(
                GL_SHADING_LANGUAGE_VERSION));
    //DBG("GlManager: Gl extensions: %1", glGetString(GL_EXTENSIONS));

    int major;
    int minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    DBG("GlxPlatform: Gl version: %1.%2", major, minor);

    int samples;
    glGetIntegerv(GL_SAMPLE_BUFFERS, &samples);
    DBG("GlxPlatform: samples: %1", samples);

    int textureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureSize);
    DBG("GlxPlatform: Maximum texture size: %1x%2", textureSize, textureSize);

    int textureUnits;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureUnits);
    DBG("GlxPlatform: Maximum texture units: %1", textureUnits);
    textureUnits = 0;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &textureUnits);
    DBG("GlxPlatform: Maximum vertex texture units: %1", textureUnits);
}

GlxPlatform::GlxPlatform() :
    deferred_(new GlxPlatformDeferred(*this))
{
    int numReturned;

    try
    {
        if (!XSupportsLocale())
            throw std::runtime_error("X server doesn't support locale");

        if (XSetLocaleModifiers("@im=none") == NULL)
            throw std::runtime_error("Unable to set input method to none");

        d()->dpy_ = XOpenDisplay(0);
        if (!d()->dpy_)
            throw std::runtime_error("Unable to open X display");

        int eventBaseReturn = 0;
        int errorBaseReturn = 0;
        int majorVersionReturn = 0;
        int minorVersionReturn = 0;
        if (XSyncQueryExtension(d()->dpy_, &eventBaseReturn, &errorBaseReturn)
                && XSyncInitialize(d()->dpy_, &majorVersionReturn,
                        &minorVersionReturn))
        {
            DBG("GlxPlatform: XSync enabled, ver: %1.%2", majorVersionReturn,
                    minorVersionReturn);

            d()->xsync_ = true;
        }

        glXCreateContextAttribsARB = (glXCreateContextAttribsArbProc)
           glXGetProcAddressARB((GLubyte const*) "glXCreateContextAttribsARB");

        d()->configs_ = glXChooseFBConfig(d()->dpy_, DefaultScreen(d()->dpy_),
                doubleBufferAttributes, &numReturned);
        d()->configCount_ = numReturned;
        if(!d()->configs_)
            throw std::runtime_error("Unable to configure glX");

        d()->dummyBuffer_ = glXCreatePbuffer(d()->dpy_, d()->configs_[0],
                dummyBufferAttributes);
        if (!d()->dummyBuffer_)
            throw std::runtime_error("Unable to create Pbuffer");
    }
    catch (...)
    {
        if (d()->configs_)
            XFree(d()->configs_);

        delete deferred_;
        throw;
    }
}

GlxPlatform::~GlxPlatform()
{
    delete deferred_;
}

Display* GlxPlatform::getDisplay()
{
    return d()->dpy_;
}

GlxPlatform::Lock GlxPlatform::lockX()
{
    ZoneScoped;
    return GlxPlatform::Lock(d()->mutex_);
}

std::vector<XEvent> GlxPlatform::getEvents(Lock const&)
{
    size_t count = XPending(d()->dpy_);
    std::vector<XEvent> events;

    // Read all events from X
    for (size_t i = 0; i < count; ++i)
    {
        events.push_back(XEvent());
        XNextEvent(d()->dpy_, &events.back());
    }

    return events;
}

float GlxPlatform::getScalingFactor() const
{
    char* factor = getenv("ASE_SCALING_FACTOR");

    if (!factor)
        return 2.0f;

    return static_cast<float>(std::max(0.25, atof(factor)));
}

inline std::optional<std::chrono::microseconds> minTime(
        std::optional<std::chrono::microseconds> const& l,
        std::optional<std::chrono::microseconds> const& r)
{
    if (l.has_value() && r.has_value())
        return std::min(*l, *r);
    else if (r.has_value())
        return r;
    else
        return l;
}

Window GlxPlatform::makeWindow(Vector2i size)
{
    auto window = std::make_shared<GlxWindow>(*this, size, getScalingFactor());

    {
        auto lock = lockX();
        d()->windows_.push_back(window);
    }

    return Window(std::move(window));
}

void GlxPlatform::handleEvents()
{
    ZoneScoped;

    std::vector<XEvent> events;
    std::vector<std::shared_ptr<GlxWindow>> windows;

    {
        Lock lock(lockX());
        events = getEvents(lock);
        windows.reserve(d()->windows_.size());

        bool needClean = false;
        for (std::weak_ptr<GlxWindow>& window : d()->windows_)
        {
            auto w = window.lock();
            if (w)
                windows.push_back(window.lock());
            else
                needClean = true;
        }

        if (needClean)
        {
            d()->windows_.erase(
                    std::remove_if(
                        d()->windows_.begin(),
                        d()->windows_.end(),
                        [](auto& w) { return w.expired(); }
                        ),
                    d()->windows_.end()
                    );
        }
    }

    for (std::shared_ptr<GlxWindow>& window : windows)
        window->handleEvents(events);
}

RenderContext GlxPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<GlxRenderContext>(*this));
}

void GlxPlatform::run(RenderContext& renderContext,
        std::function<bool(Frame const&)> frameCallback)
{
    DBG("Starting GlxPlatform::run");

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;
    auto nextFrame = startTime + std::chrono::microseconds(16667);

    std::queue<btl::future::Future<>> frameFutures;
    auto mainQueue = renderContext.getMainRenderQueue();

    while (true)
    {
        auto thisFrame = clock.now();
        auto time = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - startTime);
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);

        handleEvents();

        Frame frame { time, dt };

        if (!frameCallback(frame))
            break;

        for (auto& weakWindow : d()->windows_)
        {
            if (auto window = weakWindow.lock())
            {
                if (window->needsRedraw())
                    window->frame(frame);
            }
        }

        ase::CommandBuffer commandBuffer;
        frameFutures.push(commandBuffer.pushFence());
        mainQueue.submit(std::move(commandBuffer));

        auto now = clock.now();
        nextFrame += std::chrono::microseconds(16667);
        while (nextFrame < now)
        {
            nextFrame += std::chrono::microseconds(16667);
        }

        /*
        auto frameTime = std::chrono::duration_cast<
            std::chrono::microseconds>(now - thisFrame);
        auto remaining = nextFrame - now;
        if (remaining.count() > 0)
        {
            ZoneScopedN("sleep");
            std::this_thread::sleep_for(remaining);
        }
        */

        if (frameFutures.size() > 2)
        {
            ZoneScopedN("Wait for frame to finish");
            ZoneValue(frameFutures.size());
            frameFutures.front().wait();
            frameFutures.pop();
        }

        lastFrame = thisFrame;
    }

    while (!frameFutures.empty())
    {
        ZoneScopedN("Wait for frame to finish");
        ZoneValue(frameFutures.size());
        frameFutures.front().wait();
        frameFutures.pop();
    }

    DBG("Shutting down GlxPlatform..");
}

void GlxPlatform::requestFrame()
{
}

GLXContext createNewGlContext(Display* display, GLXContext sharedContext,
        GLXFBConfig& config, int* contextAttribs)
{
    GLXContext context = 0;

    ctxErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) =
        XSetErrorHandler(&ctxErrorHandler);

    context = glXCreateContextAttribsARB(display, config,
            sharedContext, True, contextAttribs);

    XSetErrorHandler(oldHandler);

    return context;
}

GLXContext GlxPlatform::createGlxContext(GlxPlatform::Lock const& /*lock*/)
{
    static int contextAttribsGl3[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        //GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
        None
    };

    static int contextAttribsGl4[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 0,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        //GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
        None
    };

    GLXContext context = 0;
    GLXContext sharedContext = 0;
    if (!d()->glxContexts_.empty())
        sharedContext = *d()->glxContexts_.begin();

    if (d()->gl4Enabled_)
    {
        context = createNewGlContext(d()->dpy_, sharedContext, d()->configs_[0],
                contextAttribsGl4);

        if (!context)
        {
            d()->gl4Enabled_ = false;
            DBG("GlxPlatform: Failed to create OpenGl 4.0 context");
        }
        else if (d()->glxContexts_.empty())
            DBG("GlxPlatform: OpenGl 4.0 enabled");
    }

    if (!d()->gl4Enabled_ && d()->gl3Enabled_)
    {
        context = createNewGlContext(d()->dpy_, sharedContext, d()->configs_[0],
                contextAttribsGl3);

        if (!context)
        {
            d()->gl3Enabled_ = false;
            DBG("GlxPlatform: Failed to create OpenGl 3.0 context");
        }
        else if (d()->glxContexts_.empty())
            DBG("GlxPlatform: OpenGl 3.3 enabled");
    }

    if (!d()->gl4Enabled_ && !d()->gl3Enabled_)
    {
        // Create OpenGl 2.0 context
        context = glXCreateNewContext(d()->dpy_, d()->configs_[0],
                GLX_RGBA_TYPE, sharedContext, True);
        if (context && d()->glxContexts_.empty())
            DBG("GlxPlatform: OpenGl 2.0 enabled");
    }

    if (!context)
        throw std::runtime_error("GlxPlatform: Unable to create OpenGl context");

    try
    {
        d()->glxContexts_.insert(context);
    }
    catch (...)
    {
        glXDestroyContext(d()->dpy_, context);
        DBG("GlxPlatform: Out of memory");
        throw;
    }

    return context;
}

void GlxPlatform::destroyGlxContext(GlxPlatform::Lock const& /*lock*/,
        GLXContext context)
{
    auto i = d()->glxContexts_.find(context);
    if (i == d()->glxContexts_.end())
    {
        DBG("GlxPlatform: Trying to destroy unknown GLXContext");
        throw std::runtime_error("GlxPlatform: Trying to destroy unknown "
                "GLXContext");
    }
    d()->glxContexts_.erase(i);

    glXDestroyContext(d()->dpy_, context);
    DBG("GlxPlatform: Destroyed context %1", context);
}

void GlxPlatform::makeGlxContextCurrent(GlxPlatform::Lock const& /*lock*/,
        GLXContext context, GLXDrawable drawable)
{
    ZoneScoped;
    if (!drawable && context)
        drawable = d()->dummyBuffer_;

    glXMakeContextCurrent(d()->dpy_, drawable, drawable, context);
}

void GlxPlatform::swapGlxBuffers(Lock const& /*lock*/, GLXDrawable drawable)
{
    ZoneScoped;
    glXSwapBuffers(d()->dpy_, drawable);
}

void printConfig(Display* dpy, GLXFBConfig& config)
{
    int result = 0;

    glXGetFBConfigAttrib(dpy, config, GLX_FBCONFIG_ID, &result);
    DBG("\tId: %1", result);

    glXGetFBConfigAttrib(dpy, config, GLX_RED_SIZE, &result);
    DBG("\tRed size: %1", result);

    glXGetFBConfigAttrib(dpy, config, GLX_GREEN_SIZE, &result);
    DBG("\tGreen size: %1", result);

    glXGetFBConfigAttrib(dpy, config, GLX_BLUE_SIZE, &result);
    DBG("\tBlue size: %1", result);

    glXGetFBConfigAttrib(dpy, config, GLX_DEPTH_SIZE, &result);
    DBG("\tDepth size: %1", result);

    glXGetFBConfigAttrib(dpy, config, GLX_SAMPLES, &result);
    DBG("\tSamples: %1", result);

#ifdef GL_EXT_framebuffer_sRGB
    glXGetFBConfigAttrib(dpy, config, GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, &result);
    DBG("\tSrgb: %1", result ? "true" : "false");
#endif
}

GLXFBConfig GlxPlatform::getGlxFbConfig() const
{
    size_t best = 0;
    int samples;
    int bestSamples = 0;
    int hasSrgb = 0;
    bool bestHasSrgb = false;

    for (size_t i = 0; i < d()->configCount_; ++i)
    {
        glXGetFBConfigAttrib(d()->dpy_, d()->configs_[i], GLX_SAMPLES,
                &samples);
#ifdef GL_EXT_framebuffer_sRGB
        glXGetFBConfigAttrib(d()->dpy_, d()->configs_[i],
                GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, &hasSrgb);
#endif
        //printConfig(d()->dpy_, d()->configs_[i]);

        if (bestHasSrgb && !hasSrgb)
            continue;

        if (bestHasSrgb == hasSrgb && bestSamples >= samples)
            continue;

        best = i;
        bestSamples = samples;
        bestHasSrgb = hasSrgb;
    }

    //DBG("GlxPlatform: selected config:");
    //printConfig(d()->dpy_, d()->configs_[best]);

    return d()->configs_[best];
}

} // namespace

