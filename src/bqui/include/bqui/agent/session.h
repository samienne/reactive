#pragma once

#include "bqui/agent/transport.h"

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <avg/rendertree/snapshot.h>

#include <ase/keycode.h>
#include <ase/keyevent.h>
#include <ase/pointerbuttonevent.h>
#include <ase/vector.h>

#include <btl/uniqueid.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bqui::agent
{
    /**
     * @brief One window an agent observes and drives.
     *
     * The session talks to a window only through this: inject events at
     * window-space coordinates (the ones an agent reads back from an obb), read
     * the resolved introspection, and advance the window one frame with an
     * agent-supplied dt. `App` provides the concrete implementation over its
     * windows. Identity is the `id` alone; a title is content, carried inside
     * the introspection, never on the wire as identity.
     */
    class BQUI_EXPORT AgentWindow
    {
    public:
        virtual ~AgentWindow();

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

        /** @brief The current render-tree snapshot, in window space. */
        virtual avg::Snapshot snapshot() const = 0;

        /** @brief Advance this window one frame by `dt` (update then draw). */
        virtual void advance(std::chrono::microseconds dt) = 0;
    };

    /**
     * @brief The windows a session drives, as non-owning references.
     *
     * The session borrows the windows for the duration of the call; it never
     * owns them. The caller owns the adapters and keeps them alive across the
     * whole `runSession` call, so a reference (never null, never reseated)
     * states the contract more precisely than a pointer would.
     */
    using AgentWindows = std::vector<std::reference_wrapper<AgentWindow>>;

    /**
     * @brief The live app a session drives, as two hooks into its frame loop.
     *
     * A session owns neither the windows nor the clock; it borrows both from
     * the app through this. The window set is not frozen at the start — it
     * reconciles as windows open and close during the session, so the agent
     * sees exactly the windows the app currently holds.
     *
     * `reconcile` runs one fused app frame by the agent-supplied dt (update the
     * window array, then collect the new glue set only if the identities
     * changed). `liveWindows` returns adapters over the app's current windows;
     * each call rebuilds them, so a returned set is valid only until the next
     * call — never hold one across a `reconcile`.
     */
    struct AgentApp
    {
        std::function<void(std::chrono::microseconds dt)> reconcile;
        std::function<AgentWindows()> liveWindows;
    };

    /**
     * @brief Serve the inspector protocol over `transport` until shutdown.
     *
     * The app launches **paused**. A dedicated reader thread decodes frames off
     * `transport` into a thread-safe command queue; this (the app) thread runs a
     * single-threaded state machine over that queue and is the only one that
     * touches windows, sends responses, and emits notifications. Each frame
     * carries one JSON-RPC 2.0 message, routed by `method` through the registry
     * to a handler and answered with the client's `id` — a `result`, or an
     * `error` with a standard code (-32700 parse, -32600 invalid request, -32601
     * method not found, -32602 invalid params, -32603 internal). A message
     * without an `id` is a notification and draws no reply.
     *
     * `app.run` free-runs frames (advance every live window, then reconcile one
     * fused app frame) until `app.pause`; `app.step` advances a bounded `count`
     * and stays paused. The queue is drained at a frame boundary either way, so
     * a query sees a whole, untorn frame whether running or paused. `app.shutdown`
     * (or a clean channel close) ends the loop: this closes the transport to
     * wake and join the reader thread, then returns so the caller releases the
     * windows. See `system.describe` for the full registry.
     */
    BQUI_EXPORT void runSession(AgentApp const& app, Transport& transport);

    /** @brief Connect to `endpoint` and run a session over the live app. */
    BQUI_EXPORT void runSession(AgentApp const& app,
            std::string const& endpoint);
} // namespace bqui::agent
