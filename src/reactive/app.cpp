#include "app.h"

#include "signal/updateresult.h"
#include "window.h"
#include "send.h"
#include "debug.h"

#include "reactive/signal/input.h"

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

#include <pmr/statistics_resource.h>
#include <pmr/unsynchronized_pool_resource.h>
#include <pmr/new_delete_resource.h>

#include <chrono>
#include <unordered_map>

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
        size_(signal::input(ase::Vector2f(800, 600))),
        widgetInstanceSignal_(window_.getWidget()(
                    widget::BuildParams{}
                    )(std::move(size_.signal)).getInstance()),
        widgetInstance_(widgetInstanceSignal_.evaluate()),
        titleSignal_(window_.getTitle().clone()),
        drawing_(memory_)
    {
        aseWindow.setVisible(true);
        aseWindow.setTitle(titleSignal_.evaluate());

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
        auto timer = std::chrono::duration_cast<std::chrono::milliseconds>(timer_);

        signal::FrameInfo frameInfo(getNextFrameId(), dt);

        auto timeToNext = widgetInstanceSignal_.updateBegin(frameInfo);
        timeToNext = signal::min(timeToNext, titleSignal_.updateBegin(frameInfo));

        timeToNext = signal::min(timeToNext, widgetInstanceSignal_.updateEnd(frameInfo));
        timeToNext = signal::min(timeToNext, titleSignal_.updateEnd(frameInfo));

        if (titleSignal_.hasChanged())
            aseWindow.setTitle(titleSignal_.evaluate());

        if (widgetInstanceSignal_.hasChanged())
        {
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

                if (input.getRequestFocus() && handle.has_value())
                {
                    if (currentHandle_.has_value())
                        currentHandle_->set(false);
                    handle->set(true);
                    currentHandle_ = handle;
                    currentKeyHandler_ = input.getKeyHandler();
                    currentTextHandler_ = input.getTextHandler();
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

        if (widgetInstanceSignal_.hasChanged()
                || (nextUpdate_ && *nextUpdate_ <= timer)
                )
        {
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

        return timeToNext;
    }

    std::optional<signal::signal_time_t> frame(std::chrono::microseconds dt)
    {
        pointerEventsOnThisFrame_ = 0;

        if (resized_)
        {
            size_.handle.set(aseWindow.getSize().cast<float>());
            painter_.setSize(aseWindow.getSize());
            resized_ = false;
        }

        timer_ += dt;

        auto timer = std::chrono::duration_cast<std::chrono::milliseconds>(timer_);

        auto timeToNext = makeTransaction(dt, std::nullopt);

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

        //if (redraw_)
        {
            painter_.paintToWindow(aseWindow, drawing_);


            painter_.presentWindow(aseWindow);

            painter_.flush();

            redraw_ = false;

            ++frames_;
        }

        if (animating_)
            return signal::signal_time_t(0);

        return timeToNext;
    }

    uint64_t getFrames() const
    {
        return frames_;
    }

    std::string getTitle() const
    {
        return window_.getTitle().evaluate();
    }

    AnySignal<widget::Instance> const& getWidgetInstanceSignal() const
    {
        return widgetInstanceSignal_;
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
    signal::Input<ase::Vector2f> size_;
    AnySignal<widget::Instance> widgetInstanceSignal_;
    widget::Instance widgetInstance_;
    AnySignal<std::string> titleSignal_;
    //RenderCache cache_;
    bool resized_ = true;
    bool redraw_ = true;
    std::unordered_map<unsigned int, std::vector<InputArea>> areas_;
    std::unordered_map<ase::KeyCode,
        std::function<void(ase::KeyEvent const&)>> keys_;
    std::optional<signal::InputHandle<bool>> currentHandle_;
    std::optional<KeyboardInput::KeyHandler> currentKeyHandler_;
    std::optional<KeyboardInput::TextHandler> currentTextHandler_;
    uint64_t frames_ = 0;
    uint32_t pointerEventsOnThisFrame_ = 0;
    std::optional<InputArea> currentHoverArea_;

    std::chrono::microseconds timer_ = std::chrono::microseconds(0);
    avg::UniqueId containerId_;
    avg::UniqueId rectId_;
    avg::RenderTree renderTree_;
    std::optional<avg::AnimationOptions> animationOptions_;
    avg::Drawing drawing_;
    std::optional<std::chrono::milliseconds> nextUpdate_;
    bool animating_ = true;
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

int App::run(AnySignal<bool> running) &&
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
    auto lastFrame = startTime;

    DBG("Reactive running...");

    auto mainQueue = context.getMainRenderQueue();

    while (running.evaluate())
    {
        auto thisFrame = clock.now();
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);

        platform.handleEvents();

        reactive::signal::FrameInfo frame{getNextFrameId(), dt};
        auto timeToNext = running.updateBegin(frame);
        auto timeToNext2 = running.updateEnd(frame);
        timeToNext = reactive::signal::min(timeToNext, timeToNext2);

        for (auto& glue : d()->windowGlues_)
        {
            auto t = glue->frame(dt);

            timeToNext = reactive::signal::min(timeToNext, t);
        }

        mainQueue.flush();

        if (timeToNext.has_value())
        {
            auto frameTime = std::chrono::duration_cast<
                std::chrono::microseconds>(clock.now() - thisFrame);
            auto remaining = *timeToNext - frameTime;
            if (remaining.count() > 0)
                std::this_thread::sleep_for(remaining);
        }

        lastFrame = thisFrame;
    }

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
    auto running = signal::input(true);
    for (auto&& w : d()->windows_)
        w = std::move(w).onClose(send(false, running.handle));

    return std::move(*this).run(running.signal);
}

void App::withAnimation(avg::AnimationOptions animationOptions,
        std::function<void()> fn)
{
    for (auto& glue : d()->windowGlues_)
    {
        glue->makeTransaction(
                std::chrono::milliseconds(0),
                std::nullopt
                );
    }

    fn();

    for (auto& glue : d()->windowGlues_)
    {
        glue->makeTransaction(
                std::chrono::milliseconds(0),
                animationOptions
                );
    }
}

void App::withAnimation(
        std::chrono::milliseconds duration,
        avg::Curve curve,
        std::function<void()> callback
        )
{
    withAnimation(
            avg::AnimationOptions{ duration, std::move(curve) },
            std::move(callback)
            );
}

void App::withAnimation(
        float seconds,
        avg::Curve curve,
        std::function<void()> callback
        )
{
    withAnimation(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::duration<float>(seconds)
                ),
            std::move(curve),
            std::move(callback)
            );
}

App app()
{
    static App application;

    return application;
}

} // namespace reactive

