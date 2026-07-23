#include "bqui/app.h"

#include "bqui/window.h"

#include "debug.h"
#include "windowlist.h"

#include <bq/signal/input.h>
#include <bq/signal/updateresult.h>
#include <bq/signal/signalcontext.h>

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
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>
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

class WindowGlue;

class BQUI_EXPORT AppDeferred
{
public:
    AppDeferred() :
        windows_(std::make_shared<WindowList>())
    {
        TracyAppInfo("Reactive application", 20);
    }

    std::shared_ptr<WindowList> windows_;

    // The app's live glues, one per open window, keyed by window id. The run
    // loop syncs this to the window collection each frame: it builds a glue
    // for any window not yet here and tears down any glue whose window has
    // left. AnimationGuard walks it to transact every open window.
    std::vector<btl::shared<WindowGlue>> windowGlues_;
};

class WindowGlue
{
public:
    WindowGlue(ase::Platform &platform, ase::RenderContext& context,
            Window window)
        : memoryPool_(pmr::new_delete_resource()),
        memoryStatistics_(&memoryPool_),
        memory_(&memoryStatistics_),
        aseWindow(platform.makeWindow(ase::Vector2i(800, 600))),
        context_(context),
        window_(std::move(window)),
        painter_(memory_, context_),
        size_(bq::signal::makeInput(ase::Vector2f(800, 600))),
        widgetInstanceSignal_(window_.getWidget()(
                    BuildParams{}
                    )(std::move(size_.signal)).getInstance()),
        widgetInstance_(widgetInstanceSignal_.evaluate<0>().get<0>()),
        titleSignal_(window_.getTitle()),
        drawing_(memory_)
    {
        aseWindow.setVisible(true);
        aseWindow.setTitle(titleSignal_.evaluate<0>().get<0>());

        aseWindow.setFrameCallback([this](ase::Frame const& frame) {
                return onFrame(frame);
                });

        aseWindow.setCloseCallback([this]()
        {
            // The glue outlives the removal by up to a frame, so a second
            // close event would otherwise run the window's callbacks again.
            if (closed_)
                return;

            closed_ = true;

            // Removing the window is what closes it, and the callbacks run
            // first so that one of them still sees the window open. Neither
            // step evaluates a signal, which is what makes closing safe from
            // an event handler.
            window_.invokeOnClose();
            window_.close();
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

    WindowGlue(WindowGlue const &) = delete;
    WindowGlue &operator=(WindowGlue const &) = delete;

    virtual ~WindowGlue()
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
        return window_.getId();
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

    // The window this glue drives. A window is a plain value the app hands the
    // glue when it opens the window and never replaces; everything the glue
    // drives per frame is the window's own title and widget signals.
    Window window_;
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


// Releases the app's glues, whatever ends the run. A glue holds an
// ase::Window and a framebuffer belonging to the render context the run
// created, so leaving one behind outlives what it is made of.
struct GlueScope
{
    ~GlueScope()
    {
        // A frame that drew a window can still be in flight, and it holds that
        // window's framebuffer.
        queue.finish();
        app.windowGlues_.clear();
    }

    AppDeferred& app;
    ase::RenderQueue& queue;
};

App::App() :
    deferred_(std::make_shared<AppDeferred>())
{
}

App& App::addWindows(std::vector<Window> windows)
{
    d()->windows_->add(std::move(windows));

    return *this;
}

App& App::addWindow(Window window)
{
    return addWindows(std::vector<Window> { std::move(window) });
}

void App::removeWindow(btl::UniqueId id)
{
    d()->windows_->remove(id);
}

std::vector<Window> App::getWindows() const
{
    return d()->windows_->get();
}

bq::signal::AnySignal<std::vector<Window>> App::getWindowsSignal() const
{
    return d()->windows_->signal();
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

    // The glues outlive this call — they are the app's, not the loop's — but
    // the platform and the render context they hold do not, so they have to be
    // released before this returns however it returns.
    GlueScope glueScope { *d(), mainQueue };

    // The running signal is the loop's only signal now. The glues are a plain
    // imperative collection synced below, not a signal, so there is nothing to
    // fuse it with; it lives in its own context and is evaluated each frame.
    auto runningContext = bq::signal::makeSignalContext(std::move(running));

    // Syncs the live glues to the window collection: builds a glue for any
    // window not yet glued, tears down any glue whose window has left. This
    // plain set-diff is what a signal-driven reconcile used to be — the window
    // collection (which close()/removeWindow update directly) is read straight,
    // once per frame.
    auto sync = [&]()
    {
        std::vector<Window> windows = d()->windows_->get();

        // The next live set: surviving glues carried over, new ones built. A
        // window that survives keeps the glue it already had.
        std::vector<btl::shared<WindowGlue>> next;
        next.reserve(windows.size());

        // Glues whose window has left. Held apart and destroyed last, after the
        // live set is the new one.
        std::vector<btl::shared<WindowGlue>> departing;

        for (auto& glue : d()->windowGlues_)
        {
            bool present = std::any_of(windows.begin(), windows.end(),
                    [&](Window const& w) { return w.getId() == glue->getId(); });

            if (present)
                next.push_back(std::move(glue));
            else
                departing.push_back(std::move(glue));
        }

        for (auto const& window : windows)
        {
            bool glued = std::any_of(next.begin(), next.end(),
                    [&](btl::shared<WindowGlue> const& g)
                    {
                        return g->getId() == window.getId();
                    });

            if (!glued)
                next.push_back(std::make_shared<WindowGlue>(platform, context,
                            window));
        }

        // A frame that drew a departing window can still be in flight, and it
        // holds that window's framebuffer, so nothing may release a glue until
        // the queue has caught up.
        if (!departing.empty())
            mainQueue.finish();

        // Swap the new complete set in, then clear the departed: destroying a
        // window runs event handlers that reach back into the live set, which
        // must be the new one by then.
        d()->windowGlues_.swap(next);
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

            // Sync the glues to the window collection after the running signal
            // has updated, so a running signal derived from the collection
            // observes it rather than intra-frame glue teardown.
            sync();

            return runningContext.evaluate<0>().get<0>();
        });

    DBG("Shutting down...");

    auto endTime = clock.now();
    std::chrono::duration<double> time = endTime - startTime;

    for (auto const& glue : d()->windowGlues_)
    {
        DBG("Window \"%1\" had FPS of %2.", glue->getTitle(),
                (double)glue->getFrames() / time.count());
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
    for (auto& glue : app_->windowGlues_)
    {
        glue->makeTransaction(
                std::chrono::milliseconds(0),
                std::nullopt
                );
    }
}

AnimationGuard::~AnimationGuard()
{
    if (!app_)
        return;

    for (auto& glue : app_->windowGlues_)
    {
        glue->makeTransaction(
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

