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
     * Wraps a real `ase::Window` and forwards its whole handle surface to it —
     * including the `getImplOfType` decorator walk, which self-matches this type
     * so the bqui side can recover the window through the handle — with two
     * exceptions: the frame callback is captured rather than forwarded (the
     * inner platform's loop never runs remotely; the session supplies the clock
     * via `advance`, and the run-loop surface `needsRedraw()`/`frame()` goes
     * unused), and the window doubles as the `RemoteWindow` the session
     * addresses by id. `bind` supplies the identity and introspection source
     * once the bqui window that owns this handle has been built.
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

        ase::WindowImpl* getImplOfType(std::type_index type) override;
        void setVisible(bool value) override;
        bool isVisible() const override;
        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;
        ase::Vector2i getSize() const override;
        float getScalingFactor() const override;
        ase::Framebuffer& getDefaultFramebuffer() override;
        void requestFrame() override;
        ase::PresentStatus present(ase::Dispatched dispatched) override;
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
        // The render-polling surface the Session would drive; unused remotely
        // (the session supplies the clock via advance()), so needsRedraw() is
        // always false and frame() runs the captured callback.
        bool needsRedraw() const override;
        std::optional<std::chrono::microseconds> frame(
                ase::Frame const& frame) override;

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
     * inner window (and tracks it in a live registry), and the other handle calls
     * forward to the inner platform. Driving frames is not the decorator's job --
     * `App` builds a `RemoteApp` over `liveWindows` and serves `runSession`; the
     * decorator only supplies the window adapters the session addresses.
     */
    class RemotePlatformImpl : public ase::PlatformImpl
    {
    public:
        explicit RemotePlatformImpl(ase::Platform inner);

        ase::PlatformImpl* getImplOfType(std::type_index type) override;
        ase::Window makeWindow(ase::RenderContext& context, ase::Vector2i size,
                bool headless) override;
        void handleEvents() override;
        ase::RenderContext makeRenderContext() override;
        ase::Session makeSession(ase::RenderContext& context) override;
        void requestFrame() override;

        /** @brief Adapters over the live window registry, rebuilt on each call;
         * the window set a remote session drives. A returned set is valid only
         * until the next call. */
        RemoteWindows liveWindows() const;

        /** @brief Drop a window from the live registry; called by the window's
         * destructor as it unmounts. */
        void unregisterWindow(RemoteWindowImpl* window);

    private:
        ase::Platform inner_;
        std::vector<RemoteWindowImpl*> registry_;
    };

    /** @brief Wrap `inner` so its windows are exposed to a remote session; the
     * caller (App) owns the endpoint and serves the session. */
    BQUI_EXPORT ase::Platform makeRemotePlatform(ase::Platform inner);
} // namespace bqui::remote
