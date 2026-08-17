// std::getenv is portable and read-only here; silence MSVC's _dupenv_s nudge.
#define _CRT_SECURE_NO_WARNINGS

#include "bqui/app.h"

#include "bqui/window.h"
#include "bqui/modifier/background.h"

#include "remote/remoteplatform.h"

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
#include <ase/session.h>
#include <ase/dummyplatform.h>

#include <btl/runloop.h>
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
#include <cstdlib>
#include <cstring>

namespace bqui
{

namespace
{
    // A truthy env var: set and neither empty nor "0".
    bool envFlag(char const* name)
    {
        char const* value = std::getenv(name);
        return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
    }

    bool envEquals(char const* name, char const* expected)
    {
        char const* value = std::getenv(name);
        return value && std::strcmp(value, expected) == 0;
    }

    // Headless (real backend, rendered offscreen) when REACTIVE_HEADLESS is
    // truthy. This is orthogonal to the dummy platform: it keeps the native GPU
    // backend and only renders offscreen.
    bool wantsHeadlessEnv()
    {
        return envFlag("REACTIVE_HEADLESS");
    }

    // The dummy (no-GPU) platform when REACTIVE_PLATFORM=dummy. Distinct from
    // headless: the dummy backend needs no GPU at all, for agent/CI use.
    bool wantsDummyPlatformEnv()
    {
        return envEquals("REACTIVE_PLATFORM", "dummy");
    }

    std::string remoteEndpointEnv()
    {
        char const* value = std::getenv("REACTIVE_REMOTE_ENDPOINT");
        return value ? std::string(value) : std::string();
    }
} // anonymous namespace

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

    // Platform choice (which backend), headless (real backend offscreen), and
    // mode (normal/remote) are orthogonal. A programmatic override wins over the
    // env var so tests need no global env.
    std::optional<ase::Platform> platformOverride_;
    std::optional<bool> headlessOverride_;
    std::optional<std::string> remoteEndpointOverride_;
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

App& App::headless(bool headless)
{
    d()->headlessOverride_ = headless;
    return *this;
}

App& App::platform(ase::Platform platform)
{
    d()->platformOverride_ = std::move(platform);
    return *this;
}

App& App::setRemoteEndpoint(std::string endpoint)
{
    d()->remoteEndpointOverride_ = std::move(endpoint);
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
    // The real-offscreen flag, orthogonal to the backend choice below; passed
    // to each WindowImpl.
    bool headless = d()->headlessOverride_.value_or(wantsHeadlessEnv());

    // The dummy (no-GPU) backend is a separate choice, selected explicitly via
    // platform() or REACTIVE_PLATFORM=dummy -- for agent/CI use where there is
    // no GPU at all. Otherwise the OS default backend runs.
    bool useDummyEnv = !d()->platformOverride_ && wantsDummyPlatformEnv();

    // The run loop the platform drives frames on. App creates it as the process
    // default (so RunLoop::getDefault() can reach it for IO) only when it builds
    // the platform itself; a caller-supplied platform already carries its own
    // injected loop, so App must not register a second default. A RunLoop is
    // non-movable, so it lives in this optional, declared before `platform` and
    // outliving it.
    std::optional<btl::RunLoop> ownedLoop;

    ase::Platform inner = [&]() -> ase::Platform
    {
        if (d()->platformOverride_)
            return *d()->platformOverride_;

        ownedLoop.emplace(btl::RunLoop::DefaultTag{});
        return useDummyEnv ? ase::makeDummyPlatform(*ownedLoop)
                           : ase::makeDefaultPlatform(*ownedLoop);
    }();

    // Platform/headless and mode (normal/remote) are orthogonal.
    // A non-empty endpoint (override or REACTIVE_REMOTE_ENDPOINT) selects remote
    // mode; an empty override forces it off regardless of the environment.
    std::string remoteEndpoint =
        d()->remoteEndpointOverride_.value_or(remoteEndpointEnv());

    // A dummy run is bounded and deterministic; REACTIVE_FRAMES caps the frame
    // budget (the default keeps such a run from spinning forever). Applied to
    // the raw inner platform, before any wrapping, so the checked
    // getImpl<DummyPlatform> resolves the concrete platform, never a decorator.
    if (useDummyEnv)
    {
        if (char const* frames = std::getenv("REACTIVE_FRAMES"))
        {
            char* end = nullptr;
            unsigned long n = std::strtoul(frames, &end, 10);
            if (end != frames)
                inner.getImpl<ase::DummyPlatform>().setMaxFrames(n);
        }
    }

    // A remote endpoint wraps the platform in a decorator that drives its
    // windows over the inspector protocol instead of a native frame loop;
    // otherwise the app runs the platform's own loop. Either way the app only
    // picks a platform and runs it -- the remote concern lives in the decorator.
    ase::Platform platform = remoteEndpoint.empty()
        ? std::move(inner)
        : remote::makeRemotePlatform(std::move(inner));

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
            {
                next.push_back(std::make_shared<WindowImpl>(platform, context,
                            std::move(mount.first), std::move(mount.second),
                            headless));
            }
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

    // The per-frame step App drives, whichever driver runs it: advance the
    // running signal by the frame's dt, reconcile the live windows, and report
    // whether the app should keep running.
    auto frameCallback = [&](ase::Frame const& aseFrame) -> bool
    {
        bq::signal::FrameInfo frame{ getNextFrameId(), aseFrame.dt };

        runningContext.update(frame);

        sync();

        return runningContext.evaluate<0>().get<0>();
    };

    if (remoteEndpoint.empty())
    {
        // Local backend: App builds the platform's Session and drives it until
        // the frame callback returns false.
        auto session = platform.makeSession(context);
        session.run(frameCallback);
    }
    else
    {
        // Remote: the client drives frames over the inspector protocol, so App
        // supplies the two hooks a session borrows -- one fused app frame per
        // client-supplied dt, and adapters over the live window registry -- and
        // serves runSession in place of a native loop.
        auto& remotePlatform = platform.getImpl<remote::RemotePlatformImpl>();

        std::chrono::microseconds appTime{ 0 };

        remote::RemoteApp remoteApp;
        remoteApp.reconcile = [&](std::chrono::microseconds dt)
        {
            appTime += dt;
            frameCallback(ase::Frame{ appTime, dt });
        };
        remoteApp.liveWindows = [&remotePlatform]
        {
            return remotePlatform.liveWindows();
        };

        remote::runSession(remoteApp, remoteEndpoint);
    }

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
