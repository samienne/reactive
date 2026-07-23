#include "bqui/app.h"

#include "bqui/window.h"
#include "bqui/modifier/background.h"

#include "debug.h"
#include "windowdata.h"

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

namespace
{
    uint64_t s_frameId_ = 0;

    uint64_t getNextFrameId()
    {
        return ++s_frameId_;
    }
} // anonymous namespace

class WindowImpl;

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
     * The channel Window::close() reaches: a window holds its app weakly and
     * closes by leaving its collection. Thread-safe, and a no-op if no window
     * here has the id. The window's impl is torn down by the run loop's next
     * sync, not here, because an OS window is released on the app thread.
     */
    void removeWindow(btl::UniqueId id);

    std::vector<Window> getWindows() const
    {
        return *windows_.read();
    }

    // The app's window collection: one imperative, thread-safe, observable set
    // of open windows. addWindow appends, removeWindow/close remove, and the
    // run loop reads it each frame. getWindowsSignal exports it for a UI that
    // follows the set; nothing derives the windows from that signal.
    bq::signal::SharedVector<Window> windows_;

    // Windows added but not yet mounted, each with the widget to mount. An OS
    // window is built on the app thread, so addWindow — callable from any
    // thread — only enqueues here, and the run loop's sync drains it and builds
    // the impls. Guarded because addWindow and the drain run on different
    // threads.
    std::mutex pendingMutex_;
    std::vector<std::pair<Window, widget::AnyWidget>> pendingMounts_;

    // The app's live impls, one per mounted window, keyed by window id. The run
    // loop syncs this each frame: it mounts a pending widget for any window not
    // yet here and tears down any impl whose window has left. AnimationGuard
    // walks it to transact every open window. App-thread-only.
    std::vector<btl::shared<WindowImpl>> windowImpls_;
};

class WindowImpl
{
public:
    WindowImpl(ase::Platform &platform, ase::RenderContext& context,
            Window window, widget::AnyWidget widget)
        : memoryPool_(pmr::new_delete_resource()),
        memoryStatistics_(&memoryPool_),
        memory_(&memoryStatistics_),
        aseWindow(platform.makeWindow(ase::Vector2i(800, 600))),
        context_(context),
        windowData_(window.data()),
        painter_(memory_, context_),
        size_(bq::signal::makeInput(ase::Vector2f(800, 600))),
        widgetInstanceSignal_((std::move(widget)
                    | modifier::background())(
                    BuildParams{}
                    )(std::move(size_.signal)).getInstance()),
        widgetInstance_(widgetInstanceSignal_.evaluate<0>().get<0>()),
        titleSignal_(windowData_->getTitle()),
        drawing_(memory_)
    {
        aseWindow.setVisible(true);
        aseWindow.setTitle(titleSignal_.evaluate<0>().get<0>());

        aseWindow.setFrameCallback([this](ase::Frame const& frame) {
                return onFrame(frame);
                });

        aseWindow.setCloseCallback([this]()
        {
            // The impl outlives the removal by up to a frame, so a second
            // close event would otherwise run the window's callbacks again.
            if (closed_)
                return;

            closed_ = true;

            // Removing the window is what closes it, and the callbacks run
            // first so that one of them still sees the window open. Neither
            // step evaluates a signal, which is what makes closing safe from
            // an event handler.
            windowData_->invokeOnClose();
            windowData_->close();
        });
        aseWindow.setResizeCallback([this]()
                {
                    resized_ = true;
                    animating_ = true;
                });

        aseWindow.setButtonCallback([this](ase::PointerButtonEvent const &e)
        {
            if (e.state == ase::ButtonState::down)
            {
                auto areas = widgetInstance_.getInputAreas();
                for (auto const &a : areas)
                {
                    if (a.acceptsButtonEvent(e))
                    {
                        a.emitButtonEvent(e);
                        areas_[e.button].push_back(a);

                        aseWindow.requestFrame();
                    }
                }
            }
            else if (e.state == ase::ButtonState::up)
            {
                for (auto const &a : areas_[e.button])
                {
                    a.emitButtonEvent(e);
                }

                areas_[e.button].clear();

                aseWindow.requestFrame();
            }

        });

        aseWindow.setPointerCallback([this](ase::PointerMoveEvent const &e)
        {
            if (currentHoverArea_.has_value() && !currentHoverArea_->contains(e.pos))
            {
                currentHoverArea_->emitHoverEvent(HoverEvent{false, false});
                currentHoverArea_ = std::nullopt;
            }

            auto areas = widgetInstance_.getInputAreas();
            for (auto const &a : areas)
            {
                if (a.contains(e.pos))
                {

                    if (!currentHoverArea_.has_value() ||
                        currentHoverArea_->getId() != a.getId())
                    {
                        if (currentHoverArea_.has_value())
                        {
                            currentHoverArea_->emitHoverEvent(HoverEvent{false, false});
                        }

                        currentHoverArea_ = a;

                        a.emitHoverEvent(HoverEvent{true, true});
                    }

                    break;
                }
            }

            bool accepted = false;
            for (auto &item : areas_)
            {
                if (!e.buttons.at(item.first - 1))
                    continue;

                std::vector<InputArea> newAreas;
                for (auto &&area : item.second)
                {
                    EventResult r = area.emitMoveEvent(e);
                    if (r == EventResult::accept)
                    {
                        newAreas.clear();
                        newAreas.emplace_back(std::move(area));
                        accepted = true;
                        break;
                    }
                    else if (r == EventResult::possible)
                    {
                        newAreas.push_back(std::move(area));
                    }
                    else if (r == EventResult::reject)
                    {
                    }
                }

                item.second = std::move(newAreas);

                if (accepted)
                    break;
            }
        });

        aseWindow.setDragCallback([](ase::PointerDragEvent const & /*e*/)
                {
                });

        aseWindow.setKeyCallback([this](ase::KeyEvent const &e)
        {
            if (currentKeyHandler_.has_value() && e.isDown())
            {
                (*currentKeyHandler_)(e);
                keys_[e.getKey()] = *currentKeyHandler_;

                aseWindow.requestFrame();
            }
            else
            {
                auto i = keys_.find(e.getKey());
                if (i == keys_.end())
                    return;
                auto f = i->second;
                keys_.erase(i);

                f(e);

                aseWindow.requestFrame();
            }
        });

        aseWindow.setTextCallback([this](ase::TextEvent const& e)
        {
            if (currentTextHandler_.has_value())
            {
                (*currentTextHandler_)(e);

                aseWindow.requestFrame();
            }
        });

        aseWindow.setHoverCallback([this](ase::HoverEvent const &e)
        {
            if (!e.hover)
            {
                if (currentHoverArea_.has_value())
                {
                    currentHoverArea_->emitHoverEvent(e);
                    currentHoverArea_ = std::nullopt;

                    aseWindow.requestFrame();
                }
            }
        });
    }

    WindowImpl(WindowImpl const &) = delete;
    WindowImpl &operator=(WindowImpl const &) = delete;

    virtual ~WindowImpl()
    {
        std::cout << "Maximum concurrent allocations: "
            << memoryStatistics_.maximum_concurrent_bytes_allocated()
            << std::endl;
    }

    std::optional<bq::signal::signal_time_t> makeTransaction(
            std::chrono::microseconds dt,
            std::optional<avg::AnimationOptions> const& animationOptions
            )
    {
        ZoneScoped;

        auto timer = std::chrono::duration_cast<std::chrono::milliseconds>(timer_);

        bq::signal::FrameInfo frameInfo(getNextFrameId(), dt);

        auto updateResult = widgetInstanceSignal_.update(frameInfo);
        updateResult = updateResult + titleSignal_.update(frameInfo);


        if (titleSignal_.didChange<0>())
            aseWindow.setTitle(titleSignal_.evaluate<0>().get<0>());

        if (widgetInstanceSignal_.didChange<0>())
        {
            ZoneScopedN("Widget instance signal evaluation");

            widgetInstance_ = widgetInstanceSignal_.evaluate<0>().get<0>();

            // If there's an area with the same id -> update
            auto areas = widgetInstance_.getInputAreas();
            for (auto&& area : areas_)
            {
                for (InputArea& area3 : area.second)
                {
                    for (InputArea& area2 : areas)
                    {
                        if (area3.getId() == area2.getId())
                        {
                            area3 = std::move(area2);
                            break;
                        }
                    }
                }
            }

            auto inputs = widgetInstance_.getKeyboardInputs();
            for (auto&& input : inputs)
            {
                auto handle = input.getFocusHandle();

                bool focus = input.getRequestFocus() || input.hasFocus();
                if (focus && handle.has_value())
                {
                    if (input.getRequestFocus())
                    {
                        if (currentHandle_.has_value())
                            currentHandle_->set(false);
                        handle->set(true);
                    }

                    currentHandle_ = handle;
                    currentKeyHandler_ = input.getKeyHandler();
                    currentTextHandler_ = input.getTextHandler();

                    if (input.getRequestFocus())
                        break;
                }
            }

            if (!currentHandle_.has_value() && !inputs.empty())
            {
                auto handle = inputs[0];
                currentHandle_ = handle.getFocusHandle();
                if (currentHandle_.has_value())
                    currentHandle_->set(true);
                currentKeyHandler_ = handle.getKeyHandler();
                currentTextHandler_ = handle.getTextHandler();
            }
        }

        if (widgetInstanceSignal_.didChange<0>()
                || (nextUpdate_ && *nextUpdate_ <= timer)
                )
        {
            ZoneScopedN("RenderTree update");
            auto [renderTree, nextUpdate] = std::move(renderTree_).update(
                    btl::clone(widgetInstance_.getRenderTree()),
                    animationOptions,
                    timer
                    );

            if (nextUpdate_ && *nextUpdate_ < timer)
                nextUpdate_ = std::nullopt;

            if (nextUpdate && *nextUpdate < timer)
                nextUpdate = std::nullopt;

            nextUpdate_ = avg::earlier(nextUpdate_, nextUpdate);

            renderTree_ = std::move(renderTree);

            animating_ = true;

            aseWindow.requestFrame();
        }

        return updateResult.nextUpdate;
    }

    std::optional<bq::signal::signal_time_t> frame(std::chrono::microseconds dt)
    {
        ZoneScoped;

        return onFrame( { timer_, dt });
    }

    std::optional<std::chrono::microseconds> onFrame(ase::Frame const& frame)
    {
        ZoneScoped;

        timer_ = frame.time;

        if (resized_)
        {
            size_.handle.set(aseWindow.getSize().cast<float>());
            painter_.setSize(aseWindow.getSize());
            resized_ = false;
        }

        auto timer = std::chrono::duration_cast<std::chrono::milliseconds>(
                timer_);

        auto timeToNext = makeTransaction(frame.dt, std::nullopt);

        if (animating_)
        {
            auto [drawing, cont] = renderTree_.draw(
                    avg::DrawContext(&painter_),
                    avg::Obb(aseWindow.getSize().cast<float>()),
                    timer
                    );

            drawing_ = std::move(drawing);
            animating_ = cont;
        }

        painter_.clearWindow(aseWindow);
        painter_.paintToWindow(aseWindow, drawing_);
        painter_.presentWindow(aseWindow);
        painter_.flush();

        ++frames_;

        if (animating_)
            return std::chrono::microseconds(0);

        return timeToNext;
    }

    btl::UniqueId getId() const
    {
        return windowData_->getId();
    }

    uint64_t getFrames() const
    {
        return frames_;
    }

    std::string getTitle() const
    {
        return titleSignal_.evaluate<0>().get<0>();
    }

    widget::Instance const& getWidgetInstance() const
    {
        return widgetInstance_;
    }

private:
    pmr::unsynchronized_pool_resource memoryPool_;
    pmr::statistics_resource memoryStatistics_;
    pmr::memory_resource* memory_;
    ase::Window aseWindow;
    ase::RenderContext& context_;

    // The persistent data of the window this impl drives. The impl owns the
    // window's contents (its widget, below) but not its identity: the data
    // outlives the impl whenever a Window naming it is still held. Everything
    // the impl drives per frame is the window's own title and widget signals.
    std::shared_ptr<WindowData> windowData_;
    avg::Painter painter_;
    bq::signal::Input<bq::signal::SignalResult<ase::Vector2f>,
        bq::signal::SignalResult<ase::Vector2f>> size_;
    bq::signal::SignalContext<bq::signal::AnySignal<widget::Instance>>
        widgetInstanceSignal_;
    widget::Instance widgetInstance_;
    bq::signal::SignalContext<bq::signal::AnySignal<std::string>> titleSignal_;
    //RenderCache cache_;
    std::unordered_map<unsigned int, std::vector<InputArea>> areas_;
    std::unordered_map<ase::KeyCode,
        std::function<void(ase::KeyEvent const&)>> keys_;
    std::optional<bq::signal::InputHandle<bool>> currentHandle_;
    std::optional<KeyboardInput::KeyHandler> currentKeyHandler_;
    std::optional<KeyboardInput::TextHandler> currentTextHandler_;
    uint64_t frames_ = 0;
    std::optional<InputArea> currentHoverArea_;

    std::chrono::microseconds timer_ = std::chrono::microseconds(0);
    avg::RenderTree renderTree_;
    std::optional<avg::AnimationOptions> animationOptions_;
    avg::Drawing drawing_;
    std::optional<std::chrono::milliseconds> nextUpdate_;
    bool animating_ = true;
    bool resized_ = true;
    bool closed_ = false;
};


// Releases the app's impls, whatever ends the run. An impl holds an
// ase::Window and a framebuffer belonging to the render context the run
// created, so leaving one behind outlives what it is made of.
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

    // Declared before the write scope so that it outlives it: a departed
    // window's data may run user code as it is released, which must not run
    // under a lock that the same code might try to take.
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
    // already open. The window's impl is torn down by the run loop's next
    // sync, which sees the window gone from the collection.
    for (auto const& window : departing)
        window.data()->clearApp();
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

    // The window is in the collection now; the widget waits here for the run
    // loop to build its impl on the app thread.
    {
        std::lock_guard<std::mutex> lock(d()->pendingMutex_);
        d()->pendingMounts_.emplace_back(std::move(window), std::move(widget));
    }

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
    // The default policy runs the app while a window is open in its own
    // collection, and stops when the last one leaves. A caller that wants
    // another — to keep running past an empty collection — passes its own
    // signal to run(running).
    return runUntil(getWindowsSignal().map(
                [](std::vector<Window> const& windows)
                {
                    return !windows.empty();
                }));
}

int App::runUntil(bq::signal::AnySignal<bool> running)
{
    ase::Platform platform = ase::makeDefaultPlatform();

    ase::RenderContext context = platform.makeRenderContext();

    auto mainQueue = context.getMainRenderQueue();

    // The impls outlive this call — they are the app's, not the loop's — but
    // the platform and the render context they hold do not, so they have to be
    // released before this returns however it returns.
    ImplScope implScope { *d(), mainQueue };

    // The running signal is the loop's only signal now. The impls are a plain
    // imperative collection synced below, not a signal, so there is nothing to
    // fuse it with; it lives in its own context and is evaluated each frame.
    auto runningContext = bq::signal::makeSignalContext(std::move(running));

    // Syncs the live impls to the window collection: mounts a pending widget's
    // impl for any window not yet mounted, tears down any impl whose window has
    // left. This plain set-diff is what a signal-driven reconcile used to be —
    // the window collection (which close()/removeWindow update directly) is
    // read straight, once per frame, and the widgets to mount are drained from
    // the pending queue addWindow fills.
    auto sync = [&]()
    {
        std::vector<Window> windows = d()->getWindows();

        // The next live set: surviving impls carried over, newly mounted ones
        // added below. A window that survives keeps the impl it already had.
        std::vector<btl::shared<WindowImpl>> next;
        next.reserve(windows.size());

        // Impls whose window has left. Held apart and destroyed last, after the
        // live set is the new one.
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

        // Drain the pending mounts and build an impl for each window still in
        // the collection that has none. A pending widget whose window has since
        // left is dropped — a re-add re-supplies one — and one whose window is
        // already mounted (a remove and re-add that has not yet cycled through
        // a teardown) is dropped too.
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

        // Swap the new complete set in, then clear the departed: destroying a
        // window runs event handlers that reach back into the live set, which
        // must be the new one by then.
        d()->windowImpls_.swap(next);
        departing.clear();
    };

    // Seed the initial windows before the first frame.
    sync();

    std::chrono::steady_clock clock;
    auto startTime = clock.now();

    DBG("Reactive running...");

    platform.run(context, [&](ase::Frame const& aseFrame) -> bool
        {
            bq::signal::FrameInfo frame{ getNextFrameId(), aseFrame.dt };

            runningContext.update(frame);

            // Sync the impls to the window collection after the running signal
            // has updated, so a running signal derived from the collection
            // observes it rather than intra-frame impl teardown.
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

