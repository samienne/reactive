#include "app.h"

#include "window.h"
#include "send.h"
#include "debug.h"

#include "signal/input.h"
#include "signal/updateresult.h"
#include "signal/signalcontext.h"

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

#include <chrono>
#include <unordered_map>
#include <memory>
#include <queue>

namespace reactive
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

class REACTIVE_EXPORT AppDeferred
{
public:
    AppDeferred()
    {
        TracyAppInfo("Reactive application", 20);
    }

    std::vector<Window> windows_;
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
        size_(signal::makeInput(ase::Vector2f(800, 600))),
        widgetInstanceSignal_(window_.getWidget()(
                    widget::BuildParams{}
                    )(std::move(size_.signal)).getInstance()),
        widgetInstance_(widgetInstanceSignal_.evaluate()),
        titleSignal_(window_.getTitle()),
        drawing_(memory_)
    {
        aseWindow.setVisible(true);
        aseWindow.setTitle(titleSignal_.evaluate());

        aseWindow.setFrameCallback([this](ase::Frame const& frame) {
                return onFrame(frame);
                });

        aseWindow.setCloseCallback([this]() { window_.invokeOnClose(); });
        aseWindow.setResizeCallback([this]()
                {
                    resized_ = true;
                    animating_ = true;
                });

        aseWindow.setRedrawCallback([this]()
                {
                    redraw_ = true;
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

                        makeTransaction(signal::signal_time_t(0), std::nullopt);
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

                makeTransaction(signal::signal_time_t(0), std::nullopt);
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

                makeTransaction(signal::signal_time_t(0), std::nullopt);
            }
            else
            {
                auto i = keys_.find(e.getKey());
                if (i == keys_.end())
                    return;
                auto f = i->second;
                keys_.erase(i);

                f(e);

                makeTransaction(signal::signal_time_t(0), std::nullopt);
            }
        });

        aseWindow.setTextCallback([this](ase::TextEvent const& e)
        {
            if (currentTextHandler_.has_value())
            {
                (*currentTextHandler_)(e);

                makeTransaction(signal::signal_time_t(0), std::nullopt);
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

                    makeTransaction(signal::signal_time_t(0), std::nullopt);
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

    std::optional<signal::signal_time_t> makeTransaction(
            std::chrono::microseconds dt,
            std::optional<avg::AnimationOptions> const& animationOptions
            )
    {
        ZoneScoped;

        auto timer = std::chrono::duration_cast<std::chrono::milliseconds>(timer_);

        signal::FrameInfo frameInfo(getNextFrameId(), dt);

        auto updateResult = widgetInstanceSignal_.update(frameInfo);
        updateResult = updateResult + titleSignal_.update(frameInfo);


        if (titleSignal_.didChange())
            aseWindow.setTitle(titleSignal_.evaluate());

        if (widgetInstanceSignal_.didChange())
        {
            ZoneScopedN("Widget instance signal evaluation");

            widgetInstance_ = widgetInstanceSignal_.evaluate();

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

        if (widgetInstanceSignal_.didChange()
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
        }

        return updateResult.nextUpdate;
    }

    std::optional<signal::signal_time_t> frame(std::chrono::microseconds dt)
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
            redraw_ = true;
        }

        painter_.clearWindow(aseWindow);

        if (redraw_)
        {
            painter_.paintToWindow(aseWindow, drawing_);

            painter_.presentWindow(aseWindow);

            painter_.flush();

            redraw_ = false;

            ++frames_;
        }

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
        return titleSignal_.evaluate();
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
    Window window_;
    avg::Painter painter_;
    signal::Input<signal::SignalResult<ase::Vector2f>,
        signal::SignalResult<ase::Vector2f>> size_;
    signal::SignalContext<widget::Instance> widgetInstanceSignal_;
    widget::Instance widgetInstance_;
    signal::SignalContext<std::string> titleSignal_;
    //RenderCache cache_;
    std::unordered_map<unsigned int, std::vector<InputArea>> areas_;
    std::unordered_map<ase::KeyCode,
        std::function<void(ase::KeyEvent const&)>> keys_;
    std::optional<signal::InputHandle<bool>> currentHandle_;
    std::optional<KeyboardInput::KeyHandler> currentKeyHandler_;
    std::optional<KeyboardInput::TextHandler> currentTextHandler_;
    uint64_t frames_ = 0;
    std::optional<InputArea> currentHoverArea_;

    std::chrono::microseconds timer_ = std::chrono::microseconds(0);
    avg::UniqueId containerId_;
    avg::UniqueId rectId_;
    avg::RenderTree renderTree_;
    std::optional<avg::AnimationOptions> animationOptions_;
    avg::Drawing drawing_;
    std::optional<std::chrono::milliseconds> nextUpdate_;
    bool animating_ = true;
    bool resized_ = true;
    bool redraw_ = true;
};


App::App() :
    deferred_(std::make_shared<AppDeferred>())
{
}

App App::windows(std::initializer_list<Window> windows) &&
{
    for (auto const& w : windows)
        d()->windows_.push_back(btl::clone(w));

    return std::move(*this);
}

int App::run(signal::AnySignal<bool> runningSignal) &&
{
    ase::Platform platform = ase::makeDefaultPlatform();

    d()->windowGlues_.reserve(d()->windows_.size());

    ase::RenderContext context = platform.makeRenderContext();

    for (auto&& w : d()->windows_)
    {
        d()->windowGlues_.push_back(std::make_shared<WindowGlue>(
            platform, context, std::move(w)));
    }

    std::chrono::steady_clock clock;
    auto startTime = clock.now();

    DBG("Reactive running...");

    auto running = signal::makeSignalContext(runningSignal);
    platform.run(context, [&](ase::Frame const& aseFrame) -> bool
        {
            reactive::signal::FrameInfo frame{ getNextFrameId(), aseFrame.dt };
            auto [timeToNext, didChange] = running.update(frame);

            return running.evaluate();
        });

    DBG("Shutting down...");

    auto endTime = clock.now();
    std::chrono::duration<double> time = endTime - startTime;

    for (auto const& glue : d()->windowGlues_)
    {
        DBG("Window \"%1\" had FPS of %2.", glue->getTitle(),
                (double)glue->getFrames() / time.count());
    }

    d()->windowGlues_.clear();

    return 0;
}

int App::run() &&
{
    auto running = signal::makeInput(true);
    for (auto&& w : d()->windows_)
        w = std::move(w).onClose(send(false, running.handle));

    return std::move(*this).run(running.signal);
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

} // namespace reactive

