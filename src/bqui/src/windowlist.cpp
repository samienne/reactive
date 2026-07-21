#include "windowlist.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace bqui
{

WindowList::WindowList() :
    array_(bq::signal::forEach(windows_.signal(),
                [](Window const& window)
                {
                    return window.getId();
                }))
{
}

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
    // Declared before the write scope so that it outlives it: destroying a
    // window runs the caller's own code, which must not run under a lock that
    // the same code might try to take.
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
}

std::vector<Window> WindowList::get() const
{
    return *windows_.read();
}

bq::signal::AnySignal<std::vector<Window>> WindowList::signal() const
{
    return windows_.signal();
}

bq::signal::ArraySignal<Window> const& WindowList::array() const
{
    return array_;
}

} // namespace bqui
