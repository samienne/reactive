//#define GLX_GLXEXT_PROTOTYPES 1

#include "glxwindow.h"

#include "glxrendercontext.h"
#include "glxcontext.h"
#include "glxplatform.h"
#include "glerror.h"

#include "windowimpl.h"
#include "window.h"
#include "rendertarget.h"
#include "rendercontext.h"
#include "dispatcher.h"

#include "debug.h"

#include <btl/option.h>

#include <GL/gl.h>
#include <GL/glxext.h>
#include <GL/glx.h>

#include <X11/extensions/sync.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>

#include <stdexcept>
#include <algorithm>
#include <fstream>

namespace ase
{

namespace
{
#if 0
static Bool waitForNotify(Display* /*dpy*/, XEvent *event, XPointer arg)
{
    return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
}
#endif

void setSyncCounter(Display* dpy, XSyncCounter counter, int64_t value)
{
    XSyncValue syncValue;
    XSyncIntsToValue(&syncValue, value & 0xffffffff, value >> 32);
    XSyncSetCounter(dpy, counter, syncValue);
}

#if 0
void printAll(std::ostream& stream)
{
    for (unsigned int sym = 0; sym < 0xffff; ++sym)
    {
        auto* name = XKeysymToString(sym);
        if (!name)
            continue;

        stream << "case KeyCode::" << name << ":\n";
        stream << "\treturn \"" << name << "\";\n";
    }
}
#endif

} // anonymous namespace

class GlxWindowDeferred : public WindowImpl
{
public:
    typedef GlxWindow::Lock Lock;

    GlxWindowDeferred(GlxPlatform& platform, GlxWindow& window,
            Vector2i const& size);
    ~GlxWindowDeferred();

    void destroy();

    // From WindowImpl
    void setVisible(bool value) override;
    bool isVisible() const override;

    void setTitle(std::string&& title) override;
    std::string const& getTitle() const override;
    void clear() override;

    // From RenderTargetImpl
    void makeCurrent(Dispatched,
            RenderContextImpl& renderContext) const override;
    bool isComplete() const override;
    Vector2i getResolution() const override;

    GlxWindow* window_;
    inline GlxWindow* q() { return window_; }
    inline GlxWindow const* q() const { return window_; }

private:
    friend class GlxWindow;

    GlxPlatform& platform_;
    ::Window xWin_ = 0;
    GLXWindow glxWin_ = 0;
    XIM xim_ = nullptr;
    XIC xic_ = nullptr;
    XID syncCounter_ = 0;
    int64_t counterValue_ = 0;

    Vector2i size_;
    bool visible_ = false;
    std::string title_;
    mutable bool dirty_ = true;

    // Mouse handling
    btl::option<Vector2f> previousMousePos_ = btl::none;
    std::array<bool, 15> buttonPressedState_ = { {
        false, false, false, false,
        false, false, false, false,
        false, false, false, false,
        false, false, false
    }};

    bool hover_ = false;

    // Text input handling
    //XComposeStatus composeStatus_;

    // Callbacks
    std::function<void()> closeCallback_;
    std::function<void()> resizeCallback_;
    std::function<void()> redrawCallback_;
    std::function<void(PointerButtonEvent const& e)> buttonCallback_;
    std::function<void(PointerMoveEvent const& e)> pointerCallback_;
    std::function<void(KeyEvent const& e)> keyCallback_;
    std::function<void(HoverEvent const& e)> hoverCallback_;

    // Atoms
    Atom wmDelete_;
    Atom wmProtocols_;
    Atom wmSyncRequest_;

    // counters
    unsigned int frames_ = 0;
};

namespace
{
    uint32_t mapXKeyStateToModifiers(int state)
    {
        bool shift = state & ShiftMask;
        bool alt = state & Mod1Mask;
        bool meta = state & Mod2Mask;
        bool control = state & ControlMask;

        return (shift ? (uint32_t)KeyModifier::Shift : 0)
            | (alt ? (uint32_t)KeyModifier::Alt : 0)
            | (meta ? (uint32_t)KeyModifier::Meta : 0)
            | (control ? (uint32_t)KeyModifier::Control : 0)
            ;
    }
}

GlxWindowDeferred::GlxWindowDeferred(GlxPlatform& platform, GlxWindow& window,
        Vector2i const& size) :
    window_(&window),
    platform_(platform),
    size_(size)
{
    unsigned int width = size[0];
    unsigned int height = size[1];

    Display* dpy = platform_.getDisplay();

    /*std::ofstream stream("keys.txt");
    printAll(stream);
    stream.close();*/

    XVisualInfo* vInfo = 0;
    XSetWindowAttributes swa;
    int swaMask;

    try
    {
        GlxPlatform::Lock lock = platform_.lockX();

        vInfo = glXGetVisualFromFBConfig(dpy, platform_.getGlxFbConfig());

        swa.border_pixel = 0;
        swa.event_mask = StructureNotifyMask | EnterWindowMask
                | LeaveWindowMask | ExposureMask | ButtonPressMask
                | ButtonReleaseMask | PointerMotionMask
                | KeyPressMask | KeyReleaseMask | SubstructureNotifyMask
                | PropertyChangeMask | KeymapStateMask;

        swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vInfo->screen),
                vInfo->visual, AllocNone);
        swa.bit_gravity = SouthWestGravity;

        swaMask = CWBorderPixel | CWColormap | CWEventMask | CWBitGravity;

        xWin_ = XCreateWindow(dpy, RootWindow(dpy, vInfo->screen), 0, 0,
                width, height, 0, vInfo->depth, InputOutput, vInfo->visual,
                swaMask, &swa);

        if (!xWin_)
            throw std::runtime_error("Unable to create X window");

        xim_ = XOpenIM(dpy, NULL, NULL, NULL);
        if (!xim_)
            throw std::runtime_error("Unable to open X input method");

        xic_ = XCreateIC(xim_, XNInputStyle,
                XIMPreeditNothing | XIMStatusNothing,
                XNClientWindow, xWin_, NULL);
        if (!xic_)
            throw std::runtime_error("Unable to create X input context");

        XSetICFocus(xic_);

        XSyncValue value;
        XSyncIntToValue(&value, 1);
        syncCounter_ = XSyncCreateCounter(dpy, value);
        Atom wmSyncRequestCounter = XInternAtom(dpy,
                "_NET_WM_SYNC_REQUEST_COUNTER", true);

        XChangeProperty(dpy, xWin_, wmSyncRequestCounter, XA_CARDINAL,
                32, PropModeReplace, (unsigned char *) &syncCounter_, 1);

        Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", true);
        Atom wmSyncRequest = XInternAtom(dpy, "_NET_WM_SYNC_REQUEST", true);

        Atom protocols[2];
        protocols[0] = wmDelete;
        protocols[1] = wmSyncRequest;
        XSetWMProtocols(dpy, xWin_, protocols, 2);

        glxWin_ = glXCreateWindow(dpy, platform_.getGlxFbConfig(), xWin_, 0);

        PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = nullptr;
        glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress(
                (unsigned char const*) "glXSwapIntervalEXT");
        if (glXSwapIntervalEXT)
            glXSwapIntervalEXT(dpy, glxWin_, 1);

        XFree(vInfo);
        vInfo = 0;

        lock.unlock();
    }
    catch (...)
    {
        if (vInfo)
        {
            GlxPlatform::Lock lock = platform_.lockX();
            XFree(vInfo);
        }

        destroy();

        throw;
    }
}

GlxWindowDeferred::~GlxWindowDeferred()
{
    DBG("GlxWindow: Rendered %1 fames", frames_);
    destroy();
}

void GlxWindowDeferred::destroy()
{
    //platform_.wait();

    GlxWindow::Lock lock(platform_.lockX());
    Display* dpy = platform_.getDisplay();

    if (syncCounter_)
    {
        XSyncDestroyCounter(dpy, syncCounter_);
        syncCounter_ = 0;
    }

    if (glxWin_)
    {
        glXDestroyWindow(dpy, glxWin_);
        glxWin_ = 0;
    }

    if (xWin_)
    {
        XDestroyWindow(dpy, xWin_);
        xWin_ = 0;
    }
}

GlxWindow::GlxWindow(GlxPlatform& platform, Vector2i const& size) :
    Window(std::make_shared<GlxWindowDeferred>(platform, *this, size))
{
    d()->size_ = size;
    Display* dpy = d()->platform_.getDisplay();

    {
        Lock lock(lockX());
        d()->wmDelete_ = XInternAtom(dpy, "WM_DELETE_WINDOW", true);
        d()->wmProtocols_ = XInternAtom(dpy, "WM_PROTOCOLS", true);
        d()->wmSyncRequest_ = XInternAtom(dpy, "_NET_WM_SYNC_REQUEST", true);
    }

}

GlxWindow::~GlxWindow()
{
}

void GlxWindow::handleEvents(std::vector<XEvent> const& events)
{
    for (auto const& e : events)
        handleEvent(e);
}

void GlxWindow::setCloseCallback(std::function<void()> func)
{
    d()->closeCallback_ = std::move(func);
}

void GlxWindow::setResizeCallback(std::function<void()> func)
{
    d()->resizeCallback_ = std::move(func);
}

void GlxWindow::setRedrawCallback(std::function<void()> func)
{
    d()->redrawCallback_ = std::move(func);
}

void GlxWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const& e)> cb)
{
    d()->buttonCallback_ = std::move(cb);
}

void GlxWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    d()->pointerCallback_ = std::move(cb);
}

void GlxWindow::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    d()->keyCallback_ = std::move(cb);
}

void GlxWindow::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    d()->hoverCallback_ = std::move(cb);
}

Vector2i GlxWindow::getSize() const
{
    return d()->size_;
}

void GlxWindow::handleEvent(_XEvent const& e)
{
    // Skip events that are not for us.
    if (e.xany.window != d()->xWin_)
        return;

    auto event = e;

    if (XFilterEvent(&event, d()->xWin_))
        return;

    switch (event.type)
    {
    case Expose:
        if (event.xexpose.count == 0)
        {
            if (d()->redrawCallback_)
                d()->redrawCallback_();
        }
        break;
    case ConfigureNotify:
        //pos = Pos(event.xconfigure.x, event.xconfigure.y);
        d()->size_ = Vector2i(event.xconfigure.width,
                    event.xconfigure.height);
        if (d()->resizeCallback_)
            d()->resizeCallback_();
        break;

    case ClientMessage:
        if (event.xclient.message_type == d()->wmProtocols_
                && static_cast<Atom>(event.xclient.data.l[0])
                == d()->wmDelete_)
        {
            if (d()->closeCallback_)
                d()->closeCallback_();
        }
        else if (event.xclient.message_type == d()->wmProtocols_
                && static_cast<Atom>(event.xclient.data.l[0])
                == d()->wmSyncRequest_)
        {
            /*DBG("SyncRequest: %1 %2 %3 %4",
                    event.xclient.data.l[1],
                    event.xclient.data.l[2],
                    event.xclient.data.l[3],
                    event.xclient.data.l[4]);*/
            d()->counterValue_ = (int64_t)event.xclient.data.l[2]
                | ((int64_t)(event.xclient.data.l[3]) << 32);

        }
        break;
    case ButtonPress:
        d()->buttonPressedState_[event.xbutton.button-1] = true;

        d()->buttonCallback_(PointerButtonEvent
                {
                    0,
                    event.xbutton.button,
                    ButtonState::down,
                    Vector2f(
                            event.xbutton.x,
                            d()->size_[1] - event.xbutton.y
                            ),
                });
        break;
    case ButtonRelease:
        d()->buttonPressedState_[event.xbutton.button-1] = false;

        d()->buttonCallback_(PointerButtonEvent
                {
                    0,
                    event.xbutton.button,
                    ButtonState::up,
                    Vector2f(
                            event.xbutton.x,
                            d()->size_[1] - event.xbutton.y
                            )
                });
        break;
    case MotionNotify:
        if (d()->pointerCallback_)
        {
            Vector2f rel(0.0f, 0.0f);
            Vector2f pos(e.xmotion.x, d()->size_[1] - e.xmotion.y);

            if (d()->previousMousePos_.valid())
                rel = pos - *d()->previousMousePos_;

            d()->previousMousePos_ = btl::just(pos);

            d()->pointerCallback_(PointerMoveEvent
                {
                    pos,
                    rel,
                    d()->buttonPressedState_,
                    d()->hover_
                });
        }
    case KeyPress:
        {
        KeySym keysym;
        char buf[10] = "";
        XKeyEvent e = event.xkey;
        Status status;
        //int n  = XLookupString(&e, buf, sizeof(buf), &keysym, &d()->composeStatus_);
        //DBG("n: %1 %2", n, buf);
        int count = Xutf8LookupString(d()->xic_, &event.xkey, buf,
                sizeof(buf), &keysym, &status);
        buf[count] = 0;

        d()->keyCallback_(KeyEvent(KeyState::down,
                    static_cast<KeyCode>(XLookupKeysym(&e, 0)),
                    mapXKeyStateToModifiers(event.xkey.state), buf));
        }
        break;
    case KeyRelease:
        {
            XKeyEvent e = event.xkey;
            d()->keyCallback_(KeyEvent(KeyState::up,
                        static_cast<KeyCode>(XLookupKeysym(&e, 0)),
                        mapXKeyStateToModifiers(event.xkey.state), ""));
        }
        break;
    case PropertyNotify:
    case ReparentNotify:
    case MapNotify:
    case EnterNotify:
        d()->hover_ = true;
        if (d()->pointerCallback_)
        {
            Vector2f rel(0.0f, 0.0f);
            Vector2f pos(e.xcrossing.x, d()->size_[1] - e.xcrossing.y);

            if (d()->previousMousePos_.valid())
                rel = pos - *d()->previousMousePos_;

            d()->previousMousePos_ = btl::just(pos);

            d()->pointerCallback_(PointerMoveEvent
                {
                    pos,
                    rel,
                    d()->buttonPressedState_,
                    d()->hover_
                });
        }

        if (d()->hoverCallback_)
            d()->hoverCallback_(HoverEvent{ true });

        break;
    case LeaveNotify:
        d()->hover_ = false;
        if (d()->pointerCallback_)
        {
            Vector2f rel(0.0f, 0.0f);
            Vector2f pos(e.xcrossing.x, d()->size_[1] - e.xcrossing.y);

            if (d()->previousMousePos_.valid())
                rel = pos - *d()->previousMousePos_;

            d()->previousMousePos_ = btl::just(pos);

            d()->pointerCallback_(PointerMoveEvent
                {
                    pos,
                    rel,
                    d()->buttonPressedState_,
                    d()->hover_
                });
        }
        if (d()->hoverCallback_)
            d()->hoverCallback_(HoverEvent{ false });

        break;
    default:
        DBG("Default %1", event.type);
        break;
    }
}

void GlxWindow::present(Dispatched)
{
    ++d()->frames_;
    auto lock = d()->platform_.lockX();
    auto dpy = d()->platform_.getDisplay();

    d()->platform_.swapGlxBuffers(lock, d()->glxWin_);

    /*GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        DBG("glError: %1", glErrorToString(err));*/

    setSyncCounter(dpy, d()->syncCounter_, d()->counterValue_);
    XSync(dpy, false);
}

GlxWindow::Lock GlxWindow::lockX() const
{
    return d()->platform_.lockX();
}

void GlxWindowDeferred::setVisible(bool value)
{
    if (value == visible_)
        return;

    Lock lock = platform_.lockX();

    visible_ = value;

    //XEvent event;
    if (value)
    {
        XMapWindow(platform_.getDisplay(), xWin_);
        /*XIfEvent(d()->platform_.getDisplay(),
                &event, waitForNotify, (XPointer) d()->xWin_);*/
    }
    else
    {
        XUnmapWindow(platform_.getDisplay(), xWin_);
    }
}

bool GlxWindowDeferred::isVisible() const
{
    return visible_;
}

void GlxWindowDeferred::setTitle(std::string&& title)
{
    if (title_ == title)
        return;

    title_ = title;

    Lock lock(platform_.lockX());
    XStoreName(platform_.getDisplay(), xWin_, title_.c_str());
}

std::string const& GlxWindowDeferred::getTitle() const
{
    return title_;
}

void GlxWindowDeferred::clear()
{
    dirty_ = true;
}

void GlxWindowDeferred::makeCurrent(Dispatched,
        RenderContextImpl& renderContext) const
{
    auto& glxContext = reinterpret_cast<GlxRenderContext&>(renderContext);
    glxContext.getContext().makeCurrent(platform_.lockX(), glxWin_);
    GlFramebuffer fb = GlFramebuffer();
    fb.makeCurrent(Dispatched(), glxContext);
    glxContext.setViewport(Dispatched(), size_);
    if (dirty_)
    {
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glxContext.clear(Dispatched(),
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        dirty_ = false;
    }
}

bool GlxWindowDeferred::isComplete() const
{
    return true;
}

Vector2i GlxWindowDeferred::getResolution() const
{
    return size_;
}

void GlxWindow::makeCurrent(Lock const& lock, GlxContext const& context) const
{
    context.makeCurrent(lock, d()->glxWin_);
}

} // namespace

