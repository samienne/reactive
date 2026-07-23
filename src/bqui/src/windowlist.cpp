#include "windowlist.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace bqui
{

WindowList::WindowList() = default;

void WindowList::add(std::vector<Window> windows)
{
    auto handle = windows_.write();

    // Nothing is appended until every window has been checked, so a rejected
    // batch leaves the list as it was.
    std::vector<btl::UniqueId> ids;
    ids.reserve(handle->size() + windows.size());

    for (auto const& window : *handle)
        ids.push_back(window.getId());

    for (auto const& window : windows)
    {
        btl::UniqueId id = window.getId();

        if (std::find(ids.begin(), ids.end(), id) != ids.end())
        {
            throw std::invalid_argument("App: this window is already open. A "
                    "window and its copies are one window, and one window "
                    "cannot be opened twice.");
        }

        if (window.getHandle().hasList())
        {
            throw std::invalid_argument("App: this window is open in another "
                    "app. A window belongs to one app, because close() has to "
                    "know which list to leave.");
        }

        ids.push_back(id);
    }

    for (auto const& window : windows)
        window.getHandle().setList(weak_from_this());

    handle->insert(handle->end(),
            std::make_move_iterator(windows.begin()),
            std::make_move_iterator(windows.end()));
}

void WindowList::remove(btl::UniqueId id)
{
    // Nothing to publish if the window is not here, and publishing is a copy
    // of every window that is. Losing the race against a concurrent removal
    // only costs one such copy, so the check needs no more than a read scope.
    {
        auto handle = windows_.read();

        auto found = std::find_if(handle->begin(), handle->end(),
                [id](Window const& window)
                {
                    return window.getId() == id;
                });

        if (found == handle->end())
            return;
    }

    // Declared before the write scope so that it outlives it: destroying a
    // window destroys its widgets, which is the caller's own code and must not
    // run under a lock that the same code might try to take. What that does
    // not reach is the copy the signal was last given, which the write scope
    // releases as it publishes — a SharedVector always copies and destroys T
    // under its lock, and says so.
    std::vector<Window> departing;

    {
        auto handle = windows_.write();

        auto departed = std::stable_partition(handle->begin(), handle->end(),
                [id](Window const& window)
                {
                    return window.getId() != id;
                });

        departing.insert(departing.end(),
                std::make_move_iterator(departed),
                std::make_move_iterator(handle->end()));

        handle->erase(departed, handle->end());
    }

    // A window that has left belongs to no app again, so it can be opened in
    // one — this one or another — without being taken for a window that is
    // already open.
    for (auto const& window : departing)
        window.getHandle().clearList();
}

std::vector<Window> WindowList::get() const
{
    return *windows_.read();
}

bq::signal::AnySignal<std::vector<Window>> WindowList::signal() const
{
    return windows_.signal();
}

} // namespace bqui
