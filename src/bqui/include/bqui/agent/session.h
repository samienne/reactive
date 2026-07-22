#pragma once

#include "bqui/agent/transport.h"

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

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
     * windows.
     */
    class BQUI_EXPORT AgentWindow
    {
    public:
        virtual ~AgentWindow();

        /** @brief This window's stable identity, its address on the wire. */
        virtual btl::UniqueId id() const = 0;

        /** @brief This window's current title. */
        virtual std::string title() const = 0;

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
     * @brief Serve the agent over `transport` until it quits or disconnects.
     *
     * Synchronous request/response over the connected transport: receive one
     * command, apply it to the live windows, send back a snapshot of them,
     * repeat. Commands are `step` (route each inject to its target window by
     * id, advance every live window one dt so click handlers run — these may
     * open or close windows — then reconcile one app frame and snapshot the
     * new set), `snapshot` (reconcile without advancing, then observe), and
     * `quit`. A malformed command is answered with an error, not a crash; a
     * clean channel close ends the loop.
     */
    BQUI_EXPORT void runSession(AgentApp const& app, Transport& transport);

    /** @brief Connect to `endpoint` and run a session over the live app. */
    BQUI_EXPORT void runSession(AgentApp const& app,
            std::string const& endpoint);
} // namespace bqui::agent
