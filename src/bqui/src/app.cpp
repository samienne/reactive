#include "bqui/app.h"

#include "bqui/window.h"
#include "bqui/modifier/background.h"

#include "debug.h"
#include "windowdata.h"
#include "windowimpl.h"

#include <bq/signal/input.h>
#include <bq/signal/updateresult.h>
#include <bq/signal/signalcontext.h>
#include <bq/signal/sharedvector.h>

#include <avg/rendertree.h>
#include <avg/painter.h>
#include <avg/rendering.h>

#include <ase/renderqueue.h>
#include <ase/commandbuffer.h>
#include <ase/window.h>
#include <ase/keyevent.h>
#include <ase/pointerbuttonevent.h>
#include <ase/rendercontext.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/platform.h>

#include <btl/future/promise.h>
#include <btl/future/future.h>
#include <btl/future/futurecontrol.h>

#include <pmr/statistics_resource.h>
#include <pmr/unsynchronized_pool_resource.h>
#include <pmr/new_delete_resource.h>

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <cstdint>

namespace bqui
{

class BQUI_EXPORT AppDeferred :
    public std::enable_shared_from_this<AppDeferred>
{
public:
    AppDeferred()
    {
        TracyAppInfo("Reactive application", 20);
    }

    /** @brief Removes the window with this id from the collection.
     *
     * Thread-safe, and a no-op if no window here has the id. The window's impl
     * is torn down by the run loop's next sync, not here, because an OS window
     * is released on the app thread.
     */
    void removeWindow(btl::UniqueId id);

    // Wakes the run loop, if one is running, so a pending window change is
    // reconciled by the next sync().
    void wakeLoop();

    std::vector<Window> getWindows() const
    {
        return *windows_.read();
    }

    bq::signal::SharedVector<Window> windows_;

    std::mutex pendingMutex_;
    std::vector<std::pair<Window, widget::AnyWidget>> pendingMounts_;

    // The platform is a runUntil local; it is published here for the duration
    // of the run so add/removeWindow can wake the loop. Guarded by
    // pendingMutex_, and nulled before the platform is destroyed.
    ase::Platform* runningPlatform_ = nullptr;

    std::vector<btl::shared<WindowImpl>> windowImpls_;
};

// Releases the app's impls, whatever ends the run. An impl holds an
// ase::Window and a framebuffer belonging to the run's render context, so
// leaving one behind would outlive what it is made of.
struct ImplScope
{
    ~ImplScope()
    {
        // A frame that drew a window can still be in flight, and it holds that
        // window's framebuffer.
        queue.finish();
        app.windowImpls_.clear();
    }

    AppDeferred& app;
    ase::RenderQueue& queue;
};

void WindowData::close() const
{
    std::shared_ptr<AppDeferred> app;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        app = app_.lock();
    }

    if (app)
        app->removeWindow(id_);
}

void AppDeferred::removeWindow(btl::UniqueId id)
{
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

    for (auto const& window : departing)
        window.data()->clearApp();

    wakeLoop();
}

void AppDeferred::wakeLoop()
{
    std::lock_guard<std::mutex> lock(pendingMutex_);
    if (runningPlatform_)
        runningPlatform_->requestFrame();
}

App::App() :
    deferred_(std::make_shared<AppDeferred>())
{
}

App& App::addWindow(Window window, widget::AnyWidget widget)
{
    {
        auto handle = d()->windows_.write();

        bool alreadyOpen = std::any_of(handle->begin(), handle->end(),
                [&](Window const& w) { return w.getId() == window.getId(); });

        if (alreadyOpen)
        {
            throw std::invalid_argument("App: this window is already open. A "
                    "window and its copies are one window, and one window "
                    "cannot be opened twice.");
        }

        if (window.data()->hasApp())
        {
            throw std::invalid_argument("App: this window is open in another "
                    "app. A window belongs to one app, because close() has to "
                    "know which app to leave.");
        }

        window.data()->setApp(d()->weak_from_this());

        handle->push_back(window);
    }

    {
        std::lock_guard<std::mutex> lock(d()->pendingMutex_);
        d()->pendingMounts_.emplace_back(std::move(window), std::move(widget));
    }

    d()->wakeLoop();

    return *this;
}

void App::removeWindow(btl::UniqueId id)
{
    d()->removeWindow(id);
}

std::vector<Window> App::getWindows() const
{
    return d()->getWindows();
}

bq::signal::AnySignal<std::vector<Window>> App::getWindowsSignal() const
{
    return d()->windows_.signal();
}

int App::run(bq::signal::AnySignal<bool> running)
{
    return runUntil(std::move(running));
}

int App::run()
{
    return runUntil(getWindowsSignal().map(
                [](std::vector<Window> const& windows)
                {
                    return !windows.empty();
                }));
}

int App::runUntil(bq::signal::AnySignal<bool> running)
{
    ase::Platform platform = ase::makeDefaultPlatform();

    // Published for the run's duration so add/removeWindow can wake the loop;
    // declared after `platform` so it is nulled under the lock before the
    // platform is destroyed.
    struct PlatformScope
    {
        ~PlatformScope()
        {
            std::lock_guard<std::mutex> lock(app.pendingMutex_);
            app.runningPlatform_ = nullptr;
        }
        AppDeferred& app;
    } platformScope { *d() };

    {
        std::lock_guard<std::mutex> lock(d()->pendingMutex_);
        d()->runningPlatform_ = &platform;
    }

    ase::RenderContext context = platform.makeRenderContext();

    auto mainQueue = context.getMainRenderQueue();

    // The impls outlive this call — they are the app's, not the loop's — but
    // the platform and render context they hold do not, so they must be
    // released before this returns, however it returns.
    ImplScope implScope { *d(), mainQueue };

    auto runningContext = bq::signal::makeSignalContext(std::move(running));

    // Wake the loop when the run condition changes, so an on-demand platform
    // re-evaluates it instead of sleeping through the change.
    runningContext.observe([this] { d()->wakeLoop(); });

    // Syncs the live impls to the window collection: mounts an impl for any
    // window not yet mounted, tears down any impl whose window has left. The
    // collection (which close()/removeWindow update directly) is read straight,
    // once per frame, and the widgets to mount are drained from the pending
    // queue addWindow fills.
    auto sync = [&]()
    {
        std::vector<Window> windows = d()->getWindows();

        std::vector<btl::shared<WindowImpl>> next;
        next.reserve(windows.size());

        std::vector<btl::shared<WindowImpl>> departing;

        for (auto& impl : d()->windowImpls_)
        {
            bool present = std::any_of(windows.begin(), windows.end(),
                    [&](Window const& w) { return w.getId() == impl->getId(); });

            if (present)
                next.push_back(std::move(impl));
            else
                departing.push_back(std::move(impl));
        }

        std::vector<std::pair<Window, widget::AnyWidget>> pending;
        {
            std::lock_guard<std::mutex> lock(d()->pendingMutex_);
            pending.swap(d()->pendingMounts_);
        }

        for (auto& mount : pending)
        {
            btl::UniqueId id = mount.first.getId();

            bool present = std::any_of(windows.begin(), windows.end(),
                    [&](Window const& w) { return w.getId() == id; });

            bool mounted = std::any_of(next.begin(), next.end(),
                    [&](btl::shared<WindowImpl> const& impl)
                    {
                        return impl->getId() == id;
                    });

            if (present && !mounted)
                next.push_back(std::make_shared<WindowImpl>(platform, context,
                            std::move(mount.first), std::move(mount.second)));
        }

        // A frame that drew a departing window can still be in flight, and it
        // holds that window's framebuffer, so nothing may release an impl until
        // the queue has caught up.
        if (!departing.empty())
            mainQueue.finish();

        d()->windowImpls_.swap(next);
        departing.clear();
    };

    sync();

    std::chrono::steady_clock clock;
    auto startTime = clock.now();

    DBG("Reactive running...");

    platform.run(context, [&](ase::Frame const& aseFrame) -> bool
        {
            bq::signal::FrameInfo frame{ getNextFrameId(), aseFrame.dt };

            runningContext.update(frame);

            sync();

            return runningContext.evaluate<0>().get<0>();
        });

    DBG("Shutting down...");

    auto endTime = clock.now();
    std::chrono::duration<double> time = endTime - startTime;

    for (auto const& impl : d()->windowImpls_)
    {
        DBG("Window \"%1\" had FPS of %2.", impl->getTitle(),
                (double)impl->getFrames() / time.count());
    }

    return 0;
}

AnimationGuard App::withAnimation(avg::AnimationOptions options)
{
    return AnimationGuard(*d(), std::move(options));
}

AnimationGuard::AnimationGuard(AppDeferred& app,
        std::optional<avg::AnimationOptions> options) :
    app_(&app),
    options_(options)
{
    for (auto& impl : app_->windowImpls_)
    {
        impl->makeTransaction(
                std::chrono::milliseconds(0),
                std::nullopt
                );
    }
}

AnimationGuard::~AnimationGuard()
{
    if (!app_)
        return;

    for (auto& impl : app_->windowImpls_)
    {
        impl->makeTransaction(
                std::chrono::milliseconds(0),
                options_
                );
    }
}

App app()
{
    static App application;

    return application;
}

}

