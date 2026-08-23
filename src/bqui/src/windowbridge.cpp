#include "windowbridge.h"

#include <tracy/Tracy.hpp>

namespace bqui
{

WindowBridge::WindowBridge(ase::Platform &platform, ase::RenderContext& context,
        Window window, widget::AnyWidget widget, bool headless)
    : memoryPool_(pmr::new_delete_resource()),
    memoryStatistics_(&memoryPool_),
    memory_(&memoryStatistics_),
    aseWindow(platform.makeWindow(context, ase::Vector2i(800, 600), headless)),
    windowData_(window.data()),
    painter_(memory_, aseWindow.getRenderContext()),
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

    auto wake = [this]
    {
        needsUpdate_ = true;
        aseWindow.requestFrame();
    };
    widgetInstanceSignal_.observe(wake);
    titleSignal_.observe(wake);

    aseWindow.setFrameCallback([this](ase::Frame const& frame)
            {
                return onFrame(frame);
            });

    aseWindow.setCloseCallback([this]()
    {
        // The impl outlives the removal by up to a frame, so a second
        // close event would otherwise run the window's callbacks again.
        if (closed_)
            return;

        closed_ = true;

        // Invoke callbacks before close() so they still see the window open.
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

WindowBridge::~WindowBridge()
{
    std::cout << "Maximum concurrent allocations: "
        << memoryStatistics_.maximum_concurrent_bytes_allocated()
        << std::endl;
}

void WindowBridge::makeTransaction(
        std::chrono::microseconds dt,
        std::optional<avg::AnimationOptions> const& animationOptions
        )
{
    ZoneScoped;

    auto updateStart = std::chrono::steady_clock::now();

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

    auto updateElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - updateStart);
    std::cout << "Update took " << updateElapsed.count() << " us, changed="
        << updateResult.didChange << std::endl;
}

std::optional<bq::signal::signal_time_t> WindowBridge::frame(
        std::chrono::microseconds dt)
{
    ZoneScoped;

    return onFrame( { timer_, dt });
}

std::optional<std::chrono::microseconds> WindowBridge::onFrame(
        ase::Frame const& frame)
{
    ZoneScopedN("onFrame");

    timer_ = frame.time;

    if (resized_)
    {
        size_.handle.set(aseWindow.getSize().cast<float>());
        painter_.setSize(aseWindow.getSize());
        resized_ = false;
    }

    auto timer = std::chrono::duration_cast<std::chrono::milliseconds>(
            timer_);

    if (needsUpdate_.exchange(false))
        makeTransaction(frame.dt, std::nullopt);

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

    return animating_ ? std::optional<std::chrono::microseconds>(std::chrono::microseconds(0))
                      : std::nullopt;
}

btl::UniqueId WindowBridge::getId() const
{
    return windowData_->getId();
}

uint64_t WindowBridge::getFrames() const
{
    return frames_;
}

std::string WindowBridge::getTitle() const
{
    return titleSignal_.evaluate<0>().get<0>();
}

widget::Instance const& WindowBridge::getWidgetInstance() const
{
    return widgetInstance_;
}

widget::Introspection WindowBridge::getResolvedIntrospection() const
{
    return widget::resolveIntrospection(widgetInstance_.getIntrospection());
}

btl::UniqueId WindowBridge::id() const
{
    return getId();
}

void WindowBridge::injectPointerButton(unsigned int pointerIndex,
        unsigned int buttonIndex, ase::Vector2f pos, ase::ButtonState state)
{
    aseWindow.injectPointerButtonEvent(pointerIndex, buttonIndex, pos, state);
}

void WindowBridge::injectPointerMove(unsigned int pointerIndex, ase::Vector2f pos)
{
    aseWindow.injectPointerMoveEvent(pointerIndex, pos);
}

void WindowBridge::injectHover(unsigned int pointerIndex, ase::Vector2f pos,
        bool state)
{
    aseWindow.injectHoverEvent(pointerIndex, pos, state);
}

void WindowBridge::injectKey(ase::KeyState state, ase::KeyCode code,
        uint32_t modifiers, std::string text)
{
    aseWindow.injectKeyEvent(state, code, modifiers, std::move(text));
}

void WindowBridge::injectText(std::string text)
{
    aseWindow.injectTextEvent(std::move(text));
}

widget::Introspection WindowBridge::introspect() const
{
    return getResolvedIntrospection();
}

} // namespace bqui
