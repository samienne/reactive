#include "bqui/app.h"

#include "bqui/window.h"

#include "bqui/agent/controlloop.h"
#include "bqui/agent/transport.h"

#include "debug.h"
#include "windowlist.h"

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
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
#include <ase/dummyplatform.h>

#include <btl/future/promise.h>
#include <btl/future/future.h>
#include <btl/future/futurecontrol.h>

#include <pmr/statistics_resource.h>
#include <pmr/unsynchronized_pool_resource.h>
#include <pmr/new_delete_resource.h>

#include <tracy/Tracy.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace bqui
{

namespace
{
    uint64_t s_frameId_ = 0;

    uint64_t getNextFrameId()
    {
        return ++s_frameId_;
    }

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

    // Headless when REACTIVE_HEADLESS is truthy or REACTIVE_PLATFORM=dummy.
    bool wantsHeadlessEnv()
    {
        return envFlag("REACTIVE_HEADLESS")
            || envEquals("REACTIVE_PLATFORM", "dummy");
    }

    // Agentic mode when REACTIVE_AGENT is truthy or REACTIVE_MODE=agent.
    // Orthogonal to the platform choice.
    bool wantsAgentEnv()
    {
        return envFlag("REACTIVE_AGENT")
            || envEquals("REACTIVE_MODE", "agent");
    }

    std::string agentEndpointEnv()
    {
        char const* value = std::getenv("REACTIVE_AGENT_ENDPOINT");
        return value ? std::string(value) : std::string();
    }
} // anonymous namespace

class WindowGlue;

using GlueSignal = bq::signal::AnySignal<btl::shared<WindowGlue>>;

class BQUI_EXPORT AppDeferred
{
public:
    AppDeferred() :
        windows_(std::make_shared<WindowList>())
    {
        TracyAppInfo("Reactive application", 20);
    }

    void addArray(bq::signal::ArraySignal<Window> windows)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (started_)
        {
            throw std::logic_error("App: a window array cannot be added once "
                    "the app is running. The arrays are joined once, at the "
                    "start; add windows to the app's own collection instead.");
        }

        arrays_.push_back(std::move(windows));
    }

    // The app's own collection first, then each caller-supplied array in the
    // order it was added. Taking this is what fixes the set of arrays.
    //
    // A caller's array carries values, because everything that builds one
    // hands the value over: the constructors take items, and forEach()'s
    // delegate returns one. The app's own collection carries the element
    // signals forEach() hands that delegate. Lifting the values to constants
    // is what makes the two one array — and a constant is what a value that
    // carries no variation of its own becomes anyway.
    bq::signal::ArraySignal<bq::signal::AnySignal<Window>> takeAllWindows()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        started_ = true;

        std::vector<bq::signal::ArraySignal<bq::signal::AnySignal<Window>>>
            parts { windows_->array() };

        for (auto const& array : arrays_)
        {
            parts.push_back(array.map([](Window const& window)
                        {
                            return bq::signal::AnySignal<Window>(
                                    bq::signal::constant(window));
                        }));
        }

        return bq::signal::concat(std::move(parts));
    }

    std::shared_ptr<WindowList> windows_;
    std::vector<btl::shared<WindowGlue>> windowGlues_;

    // Platform (headful/headless) and mode (normal/agentic) are orthogonal.
    // A programmatic override wins over the env var so tests need no global env.
    std::optional<ase::Platform> platformOverride_;
    std::optional<bool> headlessOverride_;
    std::optional<bool> agentOverride_;
    std::optional<std::string> agentEndpointOverride_;

private:
    mutable std::mutex mutex_;
    std::vector<bq::signal::ArraySignal<Window>> arrays_;
    bool started_ = false;
};

class WindowGlue
{
public:
    WindowGlue(ase::Platform &platform, ase::RenderContext& context,
            bq::signal::AnySignal<Window> window)
        : memoryPool_(pmr::new_delete_resource()),
        memoryStatistics_(&memoryPool_),
        memory_(&memoryStatistics_),
        aseWindow(platform.makeWindow(ase::Vector2i(800, 600))),
        context_(context),
        windowSignal_(std::move(window)),
        window_(windowSignal_.evaluate<0>().get<0>()),
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

    // Advance the reactive state one frame: apply pending events and
    // time-dependent signals via the transaction, producing the current
    // (stateless) render tree and introspection. Does not draw. Returns the
    // transaction's requested time-to-next-update.
    std::optional<bq::signal::signal_time_t> update(ase::Frame const& frame)
    {
        ZoneScoped;

        timer_ = frame.time;

        if (resized_)
        {
            size_.handle.set(aseWindow.getSize().cast<float>());
            painter_.setSize(aseWindow.getSize());
            resized_ = false;
        }

        return makeTransaction(frame.dt, std::nullopt);
    }

    // Draw the current render tree at the given time and present it. A pure
    // evaluation of the stateless tree — no state advance — so it can be called
    // at any time point independently of update().
    void render(std::chrono::microseconds time)
    {
        ZoneScoped;

        auto timer = std::chrono::duration_cast<std::chrono::milliseconds>(
                time);

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
    }

    std::optional<std::chrono::microseconds> onFrame(ase::Frame const& frame)
    {
        ZoneScoped;

        auto timeToNext = update(frame);

        render(frame.time);

        if (animating_)
            return std::chrono::microseconds(0);

        return timeToNext;
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

    ase::Window& getWindow()
    {
        return aseWindow;
    }

    // Advance one frame with an externally supplied dt: build the frame from a
    // controlled clock and run update then render, so an agent drives time.
    void stepFrame(std::chrono::microseconds dt)
    {
        agentTime_ += dt;
        onFrame(ase::Frame{ agentTime_, dt });
    }

    widget::Introspection getResolvedIntrospection() const
    {
        return widget::resolveIntrospection(widgetInstance_.getIntrospection());
    }

private:
    pmr::unsynchronized_pool_resource memoryPool_;
    pmr::statistics_resource memoryStatistics_;
    pmr::memory_resource* memory_;
    ase::Window aseWindow;
    ase::RenderContext& context_;

    // Read once, here, and never updated. An element of an array cannot vary
    // at a fixed identity, so there is nothing later to read; and this signal
    // is the array's, so updating it on the window's own clock is exactly what
    // a consumer with a clock of its own must not do — it would read a key
    // that may have left.
    bq::signal::SignalContext<bq::signal::AnySignal<Window>> windowSignal_;
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
    std::chrono::microseconds agentTime_ = std::chrono::microseconds(0);
    avg::RenderTree renderTree_;
    std::optional<avg::AnimationOptions> animationOptions_;
    avg::Drawing drawing_;
    std::optional<std::chrono::milliseconds> nextUpdate_;
    bool animating_ = true;
    bool resized_ = true;
    bool closed_ = false;
};

namespace
{
    class GlueAgentWindow : public agent::AgentWindow
    {
    public:
        explicit GlueAgentWindow(WindowGlue& glue) : glue_(glue) {}

        void injectPointerButton(unsigned int pointerIndex,
                unsigned int buttonIndex, ase::Vector2f pos,
                ase::ButtonState state) override
        {
            glue_.getWindow().injectPointerButtonEvent(pointerIndex,
                    buttonIndex, pos, state);
        }

        void injectPointerMove(unsigned int pointerIndex,
                ase::Vector2f pos) override
        {
            glue_.getWindow().injectPointerMoveEvent(pointerIndex, pos);
        }

        void injectHover(unsigned int pointerIndex, ase::Vector2f pos,
                bool state) override
        {
            glue_.getWindow().injectHoverEvent(pointerIndex, pos, state);
        }

        void injectKey(ase::KeyState state, ase::KeyCode code,
                uint32_t modifiers, std::string text) override
        {
            glue_.getWindow().injectKeyEvent(state, code, modifiers,
                    std::move(text));
        }

        void injectText(std::string text) override
        {
            glue_.getWindow().injectTextEvent(std::move(text));
        }

        void step(std::chrono::microseconds dt) override
        {
            glue_.stepFrame(dt);
        }

        widget::Introspection introspection() const override
        {
            return glue_.getResolvedIntrospection();
        }

    private:
        WindowGlue& glue_;
    };

    void runAgentic(WindowGlue& glue, std::string const& endpoint)
    {
        auto transport = agent::connect(endpoint);
        GlueAgentWindow window(glue);
        agent::runAgentLoop(*transport, window);
    }
} // anonymous namespace


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

App& App::platform(ase::Platform platform)
{
    d()->platformOverride_ = std::move(platform);
    return *this;
}

App& App::headless(bool headless)
{
    d()->headlessOverride_ = headless;
    return *this;
}

App& App::agentic(bool agentic)
{
    d()->agentOverride_ = agentic;
    return *this;
}

App& App::agentEndpoint(std::string endpoint)
{
    d()->agentEndpointOverride_ = std::move(endpoint);
    return *this;
}

App& App::addWindowArray(bq::signal::ArraySignal<Window> windows)
{
    d()->addArray(std::move(windows));

    return *this;
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
    // another — to count array-supplied windows too, or to keep running past an
    // empty collection — passes its own signal to run(running).
    return runUntil(getWindowsSignal().map(
                [](std::vector<Window> const& windows)
                {
                    return !windows.empty();
                }));
}

int App::runUntil(bq::signal::AnySignal<bool> running)
{
    // Platform: explicit override, else headless env, else the OS default.
    bool headless = d()->headlessOverride_.value_or(wantsHeadlessEnv());

    ase::Platform platform = d()->platformOverride_
        ? *d()->platformOverride_
        : (headless ? ase::makeDummyPlatform() : ase::makeDefaultPlatform());

    // Platform (headful/headless) and mode (normal/agentic) are orthogonal.
    bool agentic = d()->agentOverride_.value_or(wantsAgentEnv());

    // A headless run is bounded and deterministic; REACTIVE_FRAMES caps the
    // frame budget (the default keeps a headless run from spinning forever).
    if (headless && !d()->platformOverride_)
    {
        if (char const* frames = std::getenv("REACTIVE_FRAMES"))
        {
            char* end = nullptr;
            unsigned long n = std::strtoul(frames, &end, 10);
            if (end != frames)
                platform.getImpl<ase::DummyPlatform>().setMaxFrames(n);
        }
    }

    ase::RenderContext context = platform.makeRenderContext();

    // One glue per window identity: the array builds it when the identity
    // appears and destroys it when the identity leaves, so a window that
    // survives an edit keeps the glue it already had. A glue is not a signal,
    // and join() is the only way out of the array, so each one leaves as a
    // constant.
    auto makeGlue = [&platform, &context](
            bq::signal::AnySignal<Window> const& window)
    {
        btl::shared<WindowGlue> glue = std::make_shared<WindowGlue>(platform,
                context, window);

        return GlueSignal(bq::signal::constant(std::move(glue)));
    };

    // One app-level context drives the glue array (index 0) and the running
    // signal (index 1) together each frame. Per-window signal contexts still
    // live inside each glue. The glue reconcile — collect() below — runs after
    // update(), so a running signal observes the window collection (which
    // close()/removeWindow update directly) rather than intra-frame glue
    // teardown.
    auto signalContext = bq::signal::makeSignalContext(
            bq::signal::join(d()->takeAllWindows().map(makeGlue)),
            std::move(running));

    auto mainQueue = context.getMainRenderQueue();

    // The glues outlive this call — they are the app's, not the loop's — but
    // the platform and the render context they hold do not, so they have to be
    // released before this returns however it returns.
    GlueScope glueScope { *d(), mainQueue };

    // Reconciles the app's glues from the context's current glue vector
    // (index 0), run whenever the set of window identities changes.
    auto collect = [&]()
    {
        std::vector<btl::shared<WindowGlue>> departing =
            signalContext.evaluate<0>().get<0>();

        // A frame that drew a departing window can still be in flight, and it
        // holds that window's framebuffer, so nothing may release a glue until
        // the queue has caught up.
        mainQueue.finish();

        // Swap and then clear, rather than assign: destroying a window runs
        // event handlers that reach back into the live set, which must be the
        // new one by then.
        d()->windowGlues_.swap(departing);
        departing.clear();
    };

    // Seed the initial windows: the context's constructor evaluated index 0
    // once, so it already holds the initial glue vector.
    collect();

    // Agentic mode replaces the free-running loop with an observe->act->observe
    // loop the external agent drives over the channel. Platform choice is
    // orthogonal, though it is normally paired with the headless one. The glues
    // collected above are the app's windows; the agent drives the first.
    if (agentic && !d()->windowGlues_.empty())
    {
        std::string endpoint =
            d()->agentEndpointOverride_.value_or(agentEndpointEnv());

        if (!endpoint.empty())
        {
            runAgentic(*d()->windowGlues_.front(), endpoint);
            return 0;
        }
    }

    std::chrono::steady_clock clock;
    auto startTime = clock.now();

    DBG("Reactive running...");

    platform.run(context, [&](ase::Frame const& aseFrame) -> bool
        {
            bq::signal::FrameInfo frame{ getNextFrameId(), aseFrame.dt };

            signalContext.update(frame);

            // Reconcile only when the window identities change; the running
            // bool typically changes every frame and must not force a needless
            // finish()/reconcile.
            if (signalContext.didChange<0>())
                collect();

            return signalContext.evaluate<1>().get<0>();
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
    return App();
}

}

