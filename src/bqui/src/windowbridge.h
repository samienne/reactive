#pragma once

#include "windowdata.h"

#include "bqui/window.h"
#include "bqui/buildparams.h"
#include "bqui/inputarea.h"
#include "bqui/keyboardinput.h"
#include "bqui/hoverevent.h"
#include "bqui/eventresult.h"
#include "bqui/widget/instance.h"
#include "bqui/widget/introspection.h"
#include "bqui/widget/resolvedguides.h"
#include "bqui/widget/widget.h"
#include "bqui/modifier/background.h"

#include "bqui/remote/remotedriver.h"

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

class WindowBridge : public remote::RemoteWindow
{
public:
    WindowBridge(ase::Platform &platform, ase::RenderContext& context,
            Window window, widget::AnyWidget widget, bool headless = false);

    WindowBridge(WindowBridge const &) = delete;
    WindowBridge &operator=(WindowBridge const &) = delete;

    ~WindowBridge() override;

    void makeTransaction(
            std::chrono::microseconds dt,
            std::optional<avg::AnimationOptions> const& animationOptions
            );

    std::optional<bq::signal::signal_time_t> frame(
            std::chrono::microseconds dt);

    std::optional<std::chrono::microseconds> onFrame(ase::Frame const& frame);

    btl::UniqueId getId() const;

    uint64_t getFrames() const;

    std::string getTitle() const;

    widget::Instance const& getWidgetInstance() const;

    /** @brief A resolved introspection snapshot of the current widget tree, in
     * window space, for a remote driver to read after a step. */
    widget::Introspection getResolvedIntrospection() const;

    btl::UniqueId id() const override;
    void injectPointerButton(unsigned int pointerIndex,
            unsigned int buttonIndex, ase::Vector2f pos,
            ase::ButtonState state) override;
    void injectPointerMove(unsigned int pointerIndex,
            ase::Vector2f pos) override;
    void injectHover(unsigned int pointerIndex, ase::Vector2f pos,
            bool state) override;
    void injectKey(ase::KeyState state, ase::KeyCode code,
            uint32_t modifiers, std::string text) override;
    void injectText(std::string text) override;
    widget::Introspection introspect() const override;
    avg::Snapshot snapshot() const override;

private:
    pmr::unsynchronized_pool_resource memoryPool_;
    pmr::statistics_resource memoryStatistics_;
    pmr::memory_resource* memory_;
    ase::Window aseWindow;

    std::shared_ptr<WindowData> windowData_;
    avg::Painter painter_;
    bq::signal::Input<bq::signal::SignalResult<ase::Vector2f>,
        bq::signal::SignalResult<ase::Vector2f>> size_;
    bq::signal::SignalContext<bq::signal::AnySignal<widget::Instance>>
        widgetInstanceSignal_;
    widget::Instance widgetInstance_;
    bq::signal::SignalContext<bq::signal::AnySignal<std::string>> titleSignal_;
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
