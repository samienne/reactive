#pragma once

#include "windowdata.h"
#include "remote/remoteplatform.h"

#include "bqui/window.h"
#include "bqui/buildparams.h"
#include "bqui/inputarea.h"
#include "bqui/keyboardinput.h"
#include "bqui/hoverevent.h"
#include "bqui/eventresult.h"
#include "bqui/widget/instance.h"
#include "bqui/widget/introspection.h"
#include "bqui/widget/widget.h"
#include "bqui/modifier/background.h"

#include <bq/signal/input.h>
#include <bq/signal/updateresult.h>
#include <bq/signal/signalcontext.h>
#include <bq/signal/signal.h>

#include <avg/rendertree.h>
#include <avg/painter.h>
#include <avg/rendering.h>

#include <ase/window.h>
#include <ase/rendercontext.h>
#include <ase/platform.h>
#include <ase/keyevent.h>
#include <ase/pointerbuttonevent.h>

#include <pmr/statistics_resource.h>
#include <pmr/unsynchronized_pool_resource.h>
#include <pmr/new_delete_resource.h>

#include <tracy/Tracy.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace bqui
{

inline uint64_t getNextFrameId()
{
    static std::atomic<uint64_t> s_frameId{ 0 };
    return ++s_frameId;
}

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

            // Removing the window is what closes it, and the callbacks run
            // first so that one of them still sees the window open.
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

        // Hand a remote-driven window its identity and introspection source; a
        // no-op on a real backend window, whose handle is not a decorator.
        if (auto* remoteWindow = dynamic_cast<remote::RemoteWindowImpl*>(
                    &aseWindow.getImpl<ase::WindowImpl>()))
        {
            remoteWindow->bind(getId(), [this]
                    {
                        return getResolvedIntrospection();
                    });
        }
    }

    WindowImpl(WindowImpl const &) = delete;
    WindowImpl &operator=(WindowImpl const &) = delete;

    virtual ~WindowImpl()
    {
        std::cout << "Maximum concurrent allocations: "
            << memoryStatistics_.maximum_concurrent_bytes_allocated()
            << std::endl;
    }

    void makeTransaction(
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

        auto updateElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - updateStart);
        std::cout << "Update took " << updateElapsed.count() << " us, changed="
            << updateResult.didChange << std::endl;
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

    /** @brief A resolved introspection snapshot of the current widget tree, in
     * window space, for a remote driver to read after a step. */
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
    std::atomic<bool> needsUpdate_{true};
    bool animating_ = true;
    bool resized_ = true;
    bool closed_ = false;
};

} // namespace bqui
