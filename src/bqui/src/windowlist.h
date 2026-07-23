#pragma once

#include "bqui/window.h"

#include <bq/signal/sharedvector.h>
#include <bq/signal/signal.h>

#include <btl/uniqueid.h>

#include <memory>
#include <vector>

namespace bqui
{
    /** @brief The window collection an App owns.
     *
     * Its own object rather than a member of AppDeferred so that a window can
     * hold it weakly: a window outlives the app that held it whenever a widget
     * or a callback still names it, and Window::close() has to be a no-op then
     * rather than a use of freed memory.
     *
     * The contents are a bq::signal::SharedVector, so the list is safe to
     * mutate from any thread. It is an imperative collection: add(), remove()
     * and get() reach the contents directly, and signal() exports them for a UI
     * that wants to observe the set. The app's run loop reads get() each frame
     * and syncs its live windows to it; nothing derives the windows from a
     * signal.
     */
    class WindowList : public std::enable_shared_from_this<WindowList>
    {
    public:
        WindowList();

        /** @brief Appends windows, and points them at this list.
         *
         * @throws std::invalid_argument if a window is already in the list, or
         *         appears twice among the ones added. Both mean one window is
         *         being added twice, which the array would otherwise report as
         *         a duplicate key from the middle of a frame.
         */
        void add(std::vector<Window> windows);

        void remove(btl::UniqueId id);

        std::vector<Window> get() const;

        bq::signal::AnySignal<std::vector<Window>> signal() const;

    private:
        bq::signal::SharedVector<Window> windows_;
    };
} // namespace bqui
