#pragma once

#include "bqui/remote/transport.h"

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <ase/keycode.h>
#include <ase/keyevent.h>
#include <ase/platform.h>
#include <ase/pointerbuttonevent.h>
#include <ase/vector.h>

#include <btl/runloop.h>
#include <btl/uniqueid.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bqui::remote
{
    /**
     * @brief One window a remote client observes and drives.
     *
     * The driver talks to a window only through this: inject events at
     * window-space coordinates (the ones a client reads back from an obb) and
     * read the resolved introspection. Identity is the `id` alone; a title is
     * content, carried inside the introspection, never on the wire as identity.
     * Advancing a frame is app-level (see @ref RemoteApp), so it is not on this
     * per-window seam.
     */
    class BQUI_EXPORT RemoteWindow
    {
    public:
        virtual ~RemoteWindow();

        /** @brief This window's stable identity, its address on the wire. */
        virtual btl::UniqueId id() const = 0;

        virtual void injectPointerButton(unsigned int pointerIndex,
                unsigned int buttonIndex, ase::Vector2f pos,
                ase::ButtonState state) = 0;
        virtual void injectPointerMove(unsigned int pointerIndex,
                ase::Vector2f pos) = 0;
        virtual void injectHover(unsigned int pointerIndex, ase::Vector2f pos,
                bool state) = 0;
        virtual void injectKey(ase::KeyState state, ase::KeyCode code,
                uint32_t modifiers, std::string text) = 0;
        virtual void injectText(std::string text) = 0;

        /** @brief The current resolved (window-space) introspection. */
        virtual widget::Introspection introspect() const = 0;
    };

    /**
     * @brief The windows a driver reads and injects into, as non-owning
     * references.
     *
     * The driver borrows the windows for the duration of the call and never
     * owns them. The caller owns the glues and keeps them alive across the
     * whole session.
     */
    using RemoteWindows = std::vector<std::reference_wrapper<RemoteWindow>>;

    /**
     * @brief The live app a driver drives, as the universal frame primitives.
     *
     * A driver owns neither the windows nor the clock; it borrows both from the
     * app through this. The window set is not frozen at the start -- it
     * reconciles as windows open and close during the session, so the client
     * sees exactly the windows the app currently holds.
     *
     * `step` is the driver's fallback frame primitive for the token-less case:
     * it produces one deterministic app frame by the client-supplied dt and
     * reports whether the app wants to keep running. When the driver holds a
     * pause token it steps the platform's loop directly instead (see
     * `RemoteDriver`), so `step` need not be set. `sync` reconciles the window
     * set *without* advancing a frame, so a query lands on the app's current
     * windows. `liveWindows` returns the per-window seams over the app's current
     * windows; each call rebuilds them, so a returned set is valid only until
     * the next call -- never hold one across a `step` or `sync`.
     */
    struct RemoteApp
    {
        std::function<bool(std::chrono::microseconds dt)> step;
        std::function<void()> sync;
        std::function<RemoteWindows()> liveWindows;
    };

    /**
     * @brief The inspector-protocol driver: maps JSON-RPC messages to the app's
     * universal frame primitives, over a run loop it does not own.
     *
     * A thin bqui driver -- not a platform decorator. Constructed with the loop
     * the app already drives frames on, it registers `transport` as a readable
     * source on that loop and dispatches inbound commands as they arrive. It does
     * not run the loop; the caller's `Platform::run` (or a test) does, so the
     * socket source and the frame tick share one thread.
     *
     * Two modes, one transport and protocol; the only difference is whether the
     * driver holds a `pauseToken`:
     * - **Client-driven (token engaged):** the platform's auto-cadence is
     *   suspended, so the client owns the clock. `app.run` free-runs frames off a
     *   loop timer, `app.step` advances a bounded count, and both leave the app
     *   at a clean frame boundary. Deterministic (inject -> step -> observe), for
     *   headless inspection and automated testing.
     * - **Observer (no token):** the app free-runs on its own cadence and the
     *   client only introspects and injects between frames. Live, not
     *   deterministic.
     *
     * Each message is one JSON-RPC 2.0 request, routed by `method` and answered
     * with the client's `id` -- a `result`, or an `error` with a standard code
     * (-32700 parse, -32600 invalid request, -32601 method not found, -32602
     * invalid params, -32603 internal). A message without an `id` is a
     * notification and draws no reply. The loop returns to the caller when the
     * client sends `app.shutdown` or the channel closes. See `system.describe`
     * for the registry and `docs/design/inspector-protocol.md` for the
     * concurrency model.
     */
    class BQUI_EXPORT RemoteDriver
    {
    public:
        RemoteDriver(btl::RunLoop& loop, Transport& transport, RemoteApp app,
                std::optional<ase::PauseToken> pauseToken = std::nullopt);
        ~RemoteDriver();

        RemoteDriver(RemoteDriver const&) = delete;
        RemoteDriver& operator=(RemoteDriver const&) = delete;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace bqui::remote
