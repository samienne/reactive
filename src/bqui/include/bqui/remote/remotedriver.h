#pragma once

#include "bqui/remote/transport.h"

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <avg/rendertree/snapshot.h>

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

        /**
         * @brief The render-tree snapshot of the current frame, in the same
         * window-space box the window presents.
         */
        virtual avg::Snapshot snapshot() const = 0;
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

    /** @brief The live app a driver drives, as universal frame primitives.
     *
     * A driver owns neither the windows nor the clock; it borrows both through
     * this. `step` produces one deterministic frame by the client's dt (the
     * token-less fallback; with a pause token the driver steps the platform loop
     * directly). `sync` reconciles the window set without advancing a frame.
     * `liveWindows` rebuilds the per-window seams each call, so a returned set is
     * valid only until the next call -- never hold one across a `step` or `sync`.
     */
    struct RemoteApp
    {
        std::function<bool(std::chrono::microseconds dt)> step;
        std::function<void()> sync;
        std::function<RemoteWindows()> liveWindows;
    };

    /** @brief The inspector-protocol driver: maps JSON-RPC messages to the app's
     * frame primitives, over a run loop it does not own.
     *
     * Constructed with the loop the app drives frames on; it registers
     * `transport` as a readable source there and dispatches commands as they
     * arrive. The caller runs the loop, so the socket and the frame tick share
     * one thread. The loop returns when the client sends `app.shutdown` or the
     * channel closes.
     *
     * Two modes, differing only in whether the driver holds a `pauseToken`:
     * - Client-driven (token engaged): the platform's auto-cadence is suspended
     *   so the client owns the clock. Deterministic (inject -> step -> observe),
     *   for headless inspection and automated testing.
     * - Observer (no token): the app free-runs and the client only introspects
     *   and injects between frames. Live, not deterministic.
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
