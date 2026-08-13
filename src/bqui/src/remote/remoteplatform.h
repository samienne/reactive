#pragma once

#include "bqui/remote/session.h"

#include "bqui/widget/introspection.h"

#include <ase/platformimpl.h>
#include <ase/windowimpl.h>
#include <ase/platform.h>
#include <ase/window.h>

#include <btl/uniqueid.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <typeindex>
#include <vector>

namespace ase
{
    class RenderContext;
    struct Frame;
}

namespace bqui::remote
{
    class RemotePlatformImpl;

    /**
     * @brief A backend window the remote session can drive, presented to the
     * app as an ordinary `ase::WindowImpl`.
     *
     * Wraps a real `ase::Window` and forwards its whole handle surface to it,
     * with two exceptions: the frame callback is captured rather than forwarded
     * (the inner platform's loop never runs remotely — the session supplies the
     * clock), and the window doubles as the `RemoteWindow` the session addresses
     * by id. `bind` supplies the identity and introspection source once the bqui
     * window that owns this handle has been built.
     */
    class RemoteWindowImpl : public ase::WindowImpl, public RemoteWindow
    {
    public:
        RemoteWindowImpl(ase::Window inner, RemotePlatformImpl* owner);
        ~RemoteWindowImpl() override;

        RemoteWindowImpl(RemoteWindowImpl const&) = delete;
        RemoteWindowImpl& operator=(RemoteWindowImpl const&) = delete;

        /** @brief Give the window its wire identity and its introspection
         * source, once the bqui window owning this handle has been built. */
        void bind(btl::UniqueId id,
                std::function<widget::Introspection()> introspect);

        // ase::WindowImpl — forwarded to the inner window, save setFrameCallback.
        void setVisible(bool value) override;
        bool isVisible() const override;
        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;
        ase::Vector2i getSize() const override;
        float getScalingFactor() const override;
        ase::Framebuffer& getDefaultFramebuffer() override;
        void requestFrame() override;
        ase::WindowImpl* getImplOfType(std::type_index type) override;
        void setFrameCallback(
                std::function<std::optional<std::chrono::microseconds>(
                    ase::Frame const&)> func) override;
        void setCloseCallback(std::function<void()> func) override;
        void setResizeCallback(std::function<void()> func) override;
        void setButtonCallback(
                std::function<void(ase::PointerButtonEvent const&)> cb) override;
        void setPointerCallback(
                std::function<void(ase::PointerMoveEvent const&)> cb) override;
        void setDragCallback(
                std::function<void(ase::PointerDragEvent const&)> cb) override;
        void setKeyCallback(
                std::function<void(ase::KeyEvent const&)> cb) override;
        void setHoverCallback(
                std::function<void(ase::HoverEvent const&)> cb) override;
        void setTextCallback(
                std::function<void(ase::TextEvent const&)> cb) override;
        void injectPointerButtonEvent(unsigned int pointerIndex,
                unsigned int buttonIndex, ase::Vector2f pos,
                ase::ButtonState buttonState) override;
        void injectPointerMoveEvent(unsigned int pointerIndex,
                ase::Vector2f pos) override;
        void injectHoverEvent(unsigned int pointerIndex, ase::Vector2f pos,
                bool state) override;
        void injectKeyEvent(ase::KeyState keyState, ase::KeyCode keyCode,
                uint32_t modifiers, std::string text) override;
        void injectTextEvent(std::string text) override;

        // RemoteWindow — the seam the session drives.
        btl::UniqueId id() const override;
        void injectPointerButton(unsigned int pointerIndex,
                unsigned int buttonIndex, ase::Vector2f pos,
                ase::ButtonState state) override;
        void injectPointerMove(unsigned int pointerIndex,
                ase::Vector2f pos) override;
        void injectHover(unsigned int pointerIndex, ase::Vector2f pos,
                bool state) override;
        void injectKey(ase::KeyState state, ase::KeyCode code,
                uint32_t modifiers, std::string text) override;
        void injectText(std::string text) override;
        widget::Introspection introspect() const override;
        void advance(std::chrono::microseconds dt) override;

    private:
        ase::Window inner_;
        RemotePlatformImpl* owner_;
        std::function<std::optional<std::chrono::microseconds>(
                ase::Frame const&)> frameCallback_;
        btl::UniqueId id_ = btl::UniqueId(0);
        std::function<widget::Introspection()> introspect_;
        // Deterministic clock advanced only by advance(), so a remote driver
        // stepping the window frame-by-frame gets a reproducible time base.
        std::chrono::microseconds remoteTime_ = std::chrono::microseconds(0);
    };

    /**
     * @brief An `ase::Platform` that drives its windows over the remote
     * inspector protocol instead of a native frame loop.
     *
     * Wraps a real platform: `makeWindow` hands back a `RemoteWindowImpl` over an
     * inner window (and tracks it in a live registry), the other handle calls
     * forward to the inner platform, and `run` serves a `runSession` whose frames
     * come from the client rather than a display. The frame callback the app
     * passes to `run` is the session's per-frame reconcile.
     */
    class RemotePlatformImpl : public ase::PlatformImpl
    {
    public:
        RemotePlatformImpl(ase::Platform inner, std::string endpoint);

        ase::Window makeWindow(ase::Vector2i size) override;
        void handleEvents() override;
        ase::RenderContext makeRenderContext() override;
        void run(ase::RenderContext& renderContext,
                std::function<bool(ase::Frame const&)> frameCallback) override;
        void requestFrame() override;

        /** @brief Drop a window from the live registry; called by the window's
         * destructor as it unmounts. */
        void unregisterWindow(RemoteWindowImpl* window);

    private:
        ase::Platform inner_;
        std::string endpoint_;
        std::vector<RemoteWindowImpl*> registry_;
        std::chrono::microseconds appTime_ = std::chrono::microseconds(0);
    };

    /** @brief Wrap `inner` so its windows are driven remotely over `endpoint`. */
    BQUI_EXPORT ase::Platform makeRemotePlatform(ase::Platform inner,
            std::string endpoint);
} // namespace bqui::remote
