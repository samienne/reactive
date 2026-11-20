//#define GLX_GLXEXT_PROTOTYPES 1

#include "glxwindow.h"

#include "glrenderqueue.h"
#include "renderqueue.h"
#include "dispatcher.h"
#include "debug.h"

#include <tracy/Tracy.hpp>

#include <X11/cursorfont.h>
#ifdef ASE_HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#include <cstdint>
#include <stdexcept>

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

void setSyncCounter(Dispatched, Display* dpy, XSyncCounter counter,
        int64_t value)
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

struct XCursorNames
{
    // Cursor theme names, most preferred first, ending in nullptr.
    char const* themeNames[4];
    unsigned int fontShape;
};

XCursorNames getXCursorNames(CursorKind kind)
{
    switch (kind)
    {
    case CursorKind::arrow:
    case CursorKind::hidden:
        return { { "default", "left_ptr", nullptr }, XC_left_ptr };
    case CursorKind::ibeam:
        return { { "text", "xterm", nullptr }, XC_xterm };
    case CursorKind::hand:
        return { { "pointer", "hand2", "hand1", nullptr }, XC_hand2 };
    case CursorKind::crosshair:
        return { { "crosshair", "cross", nullptr }, XC_crosshair };
    case CursorKind::move:
        return { { "move", "fleur", "all-scroll", nullptr }, XC_fleur };
    case CursorKind::resizeHorizontal:
        return { { "ew-resize", "sb_h_double_arrow", "h_double_arrow",
            nullptr }, XC_sb_h_double_arrow };
    case CursorKind::resizeVertical:
        return { { "ns-resize", "sb_v_double_arrow", "v_double_arrow",
            nullptr }, XC_sb_v_double_arrow };
    case CursorKind::resizeNwSe:
        return { { "nwse-resize", "size_fdiag", "bd_double_arrow", nullptr },
            XC_bottom_right_corner };
    case CursorKind::resizeNeSw:
        return { { "nesw-resize", "size_bdiag", "fd_double_arrow", nullptr },
            XC_bottom_left_corner };
    case CursorKind::notAllowed:
        return { { "not-allowed", "crossed_circle", "forbidden", nullptr },
            XC_X_cursor };
    case CursorKind::wait:
        return { { "wait", "watch", nullptr }, XC_watch };
    case CursorKind::progress:
        return { { "progress", "left_ptr_watch", "half-busy", nullptr },
            XC_watch };
    }

    return { { "default", "left_ptr", nullptr }, XC_left_ptr };
}

::Cursor createBlankXCursor(Display* dpy, ::Window win)
{
    char data = 0;
    XColor black{};
    Pixmap pixmap = XCreateBitmapFromData(dpy, win, &data, 1, 1);
    ::Cursor cursor = XCreatePixmapCursor(dpy, pixmap, pixmap, &black, &black,
            0, 0);
    XFreePixmap(dpy, pixmap);
    return cursor;
}

::Cursor createXCursor(Display* dpy, ::Window win, CursorKind kind)
{
    if (kind == CursorKind::hidden)
        return createBlankXCursor(dpy, win);

    XCursorNames names = getXCursorNames(kind);

#ifdef ASE_HAVE_XCURSOR
    for (char const* name : names.themeNames)
    {
        if (!name)
            break;

        if (::Cursor cursor = XcursorLibraryLoadCursor(dpy, name))
            return cursor;
    }
#endif

    return XCreateFontCursor(dpy, names.fontShape);
}

} // anonymous namespace

namespace
{
    uint32_t mapXKeyStateToModifiers(unsigned int state)
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

GlxWindow::GlxWindow(GlxPlatform& platform, RenderContext& context,
        Vector2i const& size, float scalingFactor) :
    WindowBase(context, size, scalingFactor),
    platform_(platform),
    defaultFramebuffer_(std::make_shared<GlxFramebuffer>(*this))
{
    unsigned int width = static_cast<unsigned int>((float)size[0] * scalingFactor);
    unsigned int height = static_cast<unsigned int>((float)size[1] * scalingFactor);

    pixelSize_ = Vector2i(static_cast<int>(width), static_cast<int>(height));

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

        wmDelete_ = XInternAtom(dpy, "WM_DELETE_WINDOW", true);
        wmProtocols_ = XInternAtom(dpy, "WM_PROTOCOLS", true);
        wmSyncRequest_ = XInternAtom(dpy, "_NET_WM_SYNC_REQUEST", true);

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

        XDefineCursor(dpy, xWin_, getXCursor(CursorKind::arrow));

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
            glXSwapIntervalEXT(dpy, glxWin_, -1);

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

GlxWindow::~GlxWindow()
{
    DBG("GlxWindow: Rendered %1 frames", frames_);
    destroy();
}

void GlxWindow::destroy()
{
    //platform_.wait();

    GlxWindow::Lock lock(platform_.lockX());

    Display* dpy = platform_.getDisplay();

    for (::Cursor& cursor : xCursors_)
    {
        if (cursor)
        {
            XFreeCursor(dpy, cursor);
            cursor = 0;
        }
    }

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

std::optional<std::chrono::microseconds> GlxWindow::frame(Frame const& frame)
{
    auto keepAlive = shared_from_this();
    auto& framebuffer = defaultFramebuffer_.getImpl<GlxFramebuffer>();
    getRenderContext().getMainRenderQueue().getImpl<GlRenderQueue>().dispatch(
        [&framebuffer, keepAlive, size = pixelSize_]
        (Dispatched d, GlFunctions const&)
        {
            framebuffer.setPixelSize(d, size);
        });

    return genericWindow_.frame(frame);
}

void GlxWindow::handleEvents(std::vector<XEvent> const& events)
{
    for (auto const& e : events)
        handleEvent(e);
}

void GlxWindow::handleEvent(_XEvent const& e)
{
    // Skip events that are not for us.
    if (e.xany.window != xWin_)
        return;

    auto event = e;

    if (XFilterEvent(&event, xWin_))
        return;

    switch (event.type)
    {
    case Expose:
        if (event.xexpose.count == 0)
        {
            genericWindow_.requestFrame();
            platform_.requestFrame();
        }
        break;
    case ConfigureNotify:
        if (event.xconfigure.window != xWin_)
            break;

        pixelSize_ = Vector2i(event.xconfigure.width, event.xconfigure.height);
        genericWindow_.resize(
                Vector2i(
                    (float)event.xconfigure.width / genericWindow_.getScalingFactor(),
                    (float)event.xconfigure.height / genericWindow_.getScalingFactor())
                );
        break;

    case ClientMessage:
        if (event.xclient.message_type == wmProtocols_
                && static_cast<Atom>(event.xclient.data.l[0])
                == wmDelete_)
        {
            genericWindow_.notifyClose();
        }
        else if (event.xclient.message_type == wmProtocols_
                && static_cast<Atom>(event.xclient.data.l[0])
                == wmSyncRequest_)
        {
            /*DBG("SyncRequest: %1 %2 %3 %4",
                    event.xclient.data.l[1],
                    event.xclient.data.l[2],
                    event.xclient.data.l[3],
                    event.xclient.data.l[4]);*/
            counterValue_ = (int64_t)event.xclient.data.l[2]
                | ((int64_t)(event.xclient.data.l[3]) << 32);

        }
        break;
    case ButtonPress:
    {
        float x = (float)event.xbutton.x / genericWindow_.getScalingFactor();
        float y = (float)event.xbutton.y / genericWindow_.getScalingFactor();

        Vector2f pos(x, genericWindow_.getHeight() - y);

        genericWindow_.injectPointerButtonEvent(
                0, event.xbutton.button, pos, ButtonState::down);

        break;
    }
    case ButtonRelease:
    {
        float x = (float)event.xbutton.x / genericWindow_.getScalingFactor();
        float y = (float)event.xbutton.y / genericWindow_.getScalingFactor();

        Vector2f pos(x, genericWindow_.getHeight() - y);

        genericWindow_.injectPointerButtonEvent(
                0, event.xbutton.button, pos, ButtonState::up);

        break;
    }
    case MotionNotify:
    {
        float x = (float)event.xmotion.x / genericWindow_.getScalingFactor();
        float y = (float)event.xmotion.y / genericWindow_.getScalingFactor();

        genericWindow_.injectPointerMoveEvent(
                0,
                Vector2f(x, genericWindow_.getHeight() - y)
                );
        break;
    }
    case KeyPress:
    {
        KeySym keysym;
        char buf[10] = "";
        XKeyEvent e = event.xkey;
        Status status;
        //int n  = XLookupString(&e, buf, sizeof(buf), &keysym, &composeStatus_);
        //DBG("n: %1 %2", n, buf);
        int count = Xutf8LookupString(xic_, &event.xkey, buf,
                sizeof(buf), &keysym, &status);
        buf[count] = 0;

        KeyCode key = static_cast<KeyCode>(XLookupKeysym(&e, 0));

        genericWindow_.injectKeyEvent(
                KeyState::down,
                key,
                mapXKeyStateToModifiers(event.xkey.state),
                ""
                );

        if (count > 0
                && key != KeyCode::backSpace
                && key != KeyCode::Enter
                && key != KeyCode::returnKey
                )
        {
            genericWindow_.injectTextEvent(buf);
        }

        break;
    }
    case KeyRelease:
    {
        XKeyEvent e = event.xkey;
        genericWindow_.injectKeyEvent(
                KeyState::up,
                static_cast<KeyCode>(XLookupKeysym(&e, 0)),
                mapXKeyStateToModifiers(event.xkey.state),
                ""
                );

        break;
    }
    case PropertyNotify:
    case ReparentNotify:
    case MapNotify:
        break;
    case EnterNotify:
    {
        float x = (float)e.xcrossing.x / genericWindow_.getScalingFactor();
        float y = (float)e.xcrossing.y / genericWindow_.getScalingFactor();

        genericWindow_.injectHoverEvent(
                0,
                Vector2f(x, genericWindow_.getHeight() - y),
                true
                );
        break;
    }
    case LeaveNotify:
    {
        float x = (float)e.xcrossing.x / genericWindow_.getScalingFactor();
        float y = (float)e.xcrossing.y / genericWindow_.getScalingFactor();

        genericWindow_.injectHoverEvent(
                0,
                Vector2f(x, genericWindow_.getHeight() - y),
                false
                );
        break;
    }
    default:
        DBG("Default %1", event.type);
        break;
    }
}

PresentStatus GlxWindow::present()
{
    auto keepAlive = shared_from_this();
    getRenderContext().getMainRenderQueue().getImpl<GlRenderQueue>().dispatch(
        [this, keepAlive, counterValue = counterValue_]
        (Dispatched dispatched, GlFunctions const&)
        {
            ZoneScopedN("SwapBuffers");

            ++frames_;
            auto lock = platform_.lockX();
            auto dpy = platform_.getDisplay();

            platform_.swapGlxBuffers(dispatched, lock, glxWin_);

            setSyncCounter(dispatched, dpy, syncCounter_, counterValue);
            XSync(dpy, false);
        });

    return PresentStatus::Ok;
}

GlxWindow::Lock GlxWindow::lockX() const
{
    return platform_.lockX();
}

::GLXWindow GlxWindow::getGlxWindowId() const
{
    return glxWin_;
}

void GlxWindow::setVisible(bool value)
{
    if (value == visible_)
        return;

    Lock lock = platform_.lockX();

    visible_ = value;

    //XEvent event;
    if (value)
    {
        XMapWindow(platform_.getDisplay(), xWin_);
        /*XIfEvent(platform_.getDisplay(),
                &event, waitForNotify, (XPointer) xWin_);*/
    }
    else
    {
        XUnmapWindow(platform_.getDisplay(), xWin_);
    }
}

bool GlxWindow::isVisible() const
{
    return visible_;
}

void GlxWindow::setTitle(std::string&& title)
{
    if (genericWindow_.getTitle() == title)
        return;

    genericWindow_.setTitle(title);

    Lock lock(platform_.lockX());
    XStoreName(platform_.getDisplay(), xWin_, genericWindow_.getTitle().c_str());
}

std::string const& GlxWindow::getTitle() const
{
    return genericWindow_.getTitle();
}

void GlxWindow::setCursor(Cursor cursor)
{
    if (genericWindow_.getCursor() == cursor)
        return;

    WindowBase::setCursor(cursor);

    Lock lock(platform_.lockX());
    XDefineCursor(platform_.getDisplay(), xWin_,
            getXCursor(cursor.getKind()));
    XFlush(platform_.getDisplay());
}

::Cursor GlxWindow::getXCursor(CursorKind kind)
{
    ::Cursor& cursor = xCursors_[static_cast<std::size_t>(kind)];
    if (!cursor)
        cursor = createXCursor(platform_.getDisplay(), xWin_, kind);

    return cursor;
}

Framebuffer& GlxWindow::getDefaultFramebuffer()
{
    return defaultFramebuffer_;
}

void GlxWindow::requestFrame()
{
    genericWindow_.requestFrame();
    platform_.requestFrame();
}

} // namespace

