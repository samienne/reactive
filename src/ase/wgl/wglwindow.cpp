#include "wglwindow.h"

//#include "glxframebuffer.h"
//#include "glxdispatchedcontext.h"
//#include "glxrendercontext.h"
//#include "glxcontext.h"
//#include "glxplatform.h"
//#include "glerror.h"

#include "framebuffer.h"
#include "genericwindow.h"
#include "windowimpl.h"
#include "window.h"
#include "rendercontext.h"
#include "dispatcher.h"

#include "debug.h"

#include <btl/option.h>

namespace ase
{

class WglWindowDeferred : public WindowImpl
{
public:
    typedef WglWindow::Lock Lock;

    WglWindowDeferred(WglPlatform& platform, WglWindow& window,
            Vector2i const& size);
    ~WglWindowDeferred();

    void destroy();

    // From WindowImpl
    void setVisible(bool value) override;
    bool isVisible() const override;

    void setTitle(std::string&& title) override;
    std::string const& getTitle() const override;
    void clear() override;

    Vector2i getResolution() const;

    WglWindow* window_;
    inline WglWindow* q() { return window_; }
    inline WglWindow const* q() const { return window_; }

private:
    friend class WglWindow;

    WglPlatform& platform_;

    GenericWindow genericWindow_;
    Framebuffer defaultFramebuffer_;

    bool visible_ = false;

    // counters
    unsigned int frames_ = 0;
};

WglWindowDeferred::WglWindowDeferred(WglPlatform& platform, WglWindow& window,
            Vector2i const& size):
    window_(&window),
    platform_(platform),
    genericWindow_(size),
    //defaultFramebuffer_(std::make_shared<WglFramebuffer>(platform, window))
    defaultFramebuffer_(nullptr)
{
}

WglWindowDeferred::~WglWindowDeferred()
{
    destroy();
}

void WglWindowDeferred::destroy()
{
}

void WglWindowDeferred::setVisible(bool value)
{
}

bool WglWindowDeferred::isVisible() const
{
    return false;
}

void WglWindowDeferred::setTitle(std::string&& title)
{
}

std::string const& WglWindowDeferred::getTitle() const
{
    return "";
}

void WglWindowDeferred::clear()
{
}

Vector2i WglWindowDeferred::getResolution() const
{
    return Vector2i(800, 600);
}

void WglWindow::setCloseCallback(std::function<void()> func)
{
    d()->genericWindow_.setCloseCallback(std::move(func));
}

void WglWindow::setResizeCallback(std::function<void()> func)
{
    d()->genericWindow_.setResizeCallback(std::move(func));
}

void WglWindow::setRedrawCallback(std::function<void()> func)
{
    d()->genericWindow_.setRedrawCallback(std::move(func));
}

void WglWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const& e)> cb)
{
    d()->genericWindow_.setButtonCallback(std::move(cb));
}

void WglWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    d()->genericWindow_.setPointerCallback(std::move(cb));
}

void WglWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> cb)
{
    d()->genericWindow_.setDragCallback(std::move(cb));
}

void WglWindow::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    d()->genericWindow_.setKeyCallback(std::move(cb));
}

void WglWindow::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    d()->genericWindow_.setHoverCallback(std::move(cb));
}

Vector2i WglWindow::getSize() const
{
    return d()->genericWindow_.getSize();
}

Framebuffer& WglWindow::getDefaultFramebuffer()
{
    return d()->defaultFramebuffer_;
}

void WglWindow::present(Dispatched)
{
    ++d()->frames_;

    //d()->platform_.swapGlxBuffers(lock, d()->glxWin_);

    /*GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        DBG("glError: %1", glErrorToString(err));*/
}
} // namespace ase

