#include "app.h"

#include "window.h"
#include "rendering.h"
#include "send.h"
#include "debug.h"

#include "reactive/signal/input.h"

#include "avg/painter.h"

#include <ase/window.h>
#include <ase/renderqueue.h>
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

namespace reactive
{

namespace
{
    uint64_t s_frameId_ = 0;

    uint64_t getNextFrameId()
    {
        return ++s_frameId_;
    }

    uint64_t getCurrentFrameId()
    {
        return s_frameId_;
    }
} // anonymous namespace

class REACTIVE_EXPORT AppDeferred
{
public:
    std::vector<Window> windows_;
};

class WindowGlue
{
public:
  WindowGlue(ase::Platform &platform, ase::RenderContext &&context,
             Window window, avg::Painter painter)
      : memoryPool_(pmr::new_delete_resource()),
        memoryStatistics_(&memoryPool_), memory_(&memoryStatistics_),
        aseWindow(platform.makeWindow(ase::Vector2i(800, 600), 2.0f)),
        context_(std::move(context)), window_(std::move(window)),
        painter_(std::move(painter)),
        size_(signal::input(ase::Vector2f(800, 600))),
        widget_(window_.getWidget()(signal::constant(DrawContext(memory_)),
                                    std::move(size_.signal))),
        titleSignal_(window_.getTitle().clone()) {
    aseWindow.setVisible(true);
    aseWindow.setTitle(titleSignal_.evaluate());

    aseWindow.setCloseCallback([this]() { window_.invokeOnClose(); });
    aseWindow.setResizeCallback([this]() { resized_ = true; });
    aseWindow.setRedrawCallback([this]() { redraw_ = true; });

    aseWindow.setButtonCallback([this](ase::PointerButtonEvent const &e) {
      if (pointerEventsOnThisFrame_++ > 0) {
        widget_.update({getNextFrameId(), signal::signal_time_t(0)});
      }

      if (e.state == ase::ButtonState::down) {
        for (auto const &a : widget_.getInputAreas().evaluate()) {
          if (a.acceptsButtonEvent(e)) {
            a.emitButtonEvent(e);
            areas_[e.button].push_back(a);
          }
        }
      } else if (e.state == ase::ButtonState::up) {
        for (auto const &a : areas_[e.button]) {
          a.emitButtonEvent(e);
        }

        areas_[e.button].clear();
      }
    });

    aseWindow.setPointerCallback([this](ase::PointerMoveEvent const &e) {
      std::vector<InputArea> const &areas = widget_.getInputAreas().evaluate();

      if (currentHoverArea_.valid() && !currentHoverArea_->contains(e.pos)) {
        currentHoverArea_->emitHoverEvent(HoverEvent{false, false});
        currentHoverArea_ = btl::none;
      }

      for (auto const &a : areas) {
        if (a.contains(e.pos)) {

          if (!currentHoverArea_.valid() ||
              currentHoverArea_->getId() != a.getId()) {
            if (currentHoverArea_.valid()) {
              currentHoverArea_->emitHoverEvent(HoverEvent{false, false});
            }

            currentHoverArea_ = btl::just(a);

            a.emitHoverEvent(HoverEvent{true, true});
          }

          break;
        }
      }

      bool accepted = false;
      for (auto &item : areas_) {
        if (!e.buttons.at(item.first - 1))
          continue;

        std::vector<InputArea> newAreas;
        for (auto &&area : item.second) {
          EventResult r = area.emitMoveEvent(e);
          if (r == EventResult::accept) {
            newAreas.clear();
            newAreas.emplace_back(std::move(area));
            accepted = true;
            break;
          } else if (r == EventResult::possible) {
            newAreas.push_back(std::move(area));
          } else if (r == EventResult::reject) {
          }
        }

        item.second = std::move(newAreas);

        if (accepted)
          break;
      }
    });

    aseWindow.setDragCallback([](ase::PointerDragEvent const & /*e*/) {});

    aseWindow.setKeyCallback([this](ase::KeyEvent const &e) {
      if (currentHandler_.valid() && e.isDown()) {
        (*currentHandler_)(e);
        keys_[e.getKey()] = *currentHandler_;
      } else {
        auto i = keys_.find(e.getKey());
        if (i == keys_.end())
          return;
        auto f = i->second;
        keys_.erase(i);
        f(e);
      }
    });

    aseWindow.setHoverCallback([this](ase::HoverEvent const &e) {
      if (!e.hover) {
        if (currentHoverArea_.valid()) {
          currentHoverArea_->emitHoverEvent(e);
          currentHoverArea_ = btl::none;
        }
      }
    });
  }

  WindowGlue(WindowGlue const &) = delete;
  WindowGlue &operator=(WindowGlue const &) = delete;

  virtual ~WindowGlue() {
    std::cout << "Maximum concurrent allocations: "
              << memoryStatistics_.maximum_concurrent_bytes_allocated()
              << std::endl;
  }

    btl::option<signal::signal_time_t> frame(std::chrono::microseconds dt)
    {
        pointerEventsOnThisFrame_ = 0;

        if (resized_)
        {
            size_.handle.set(aseWindow.getSize().cast<float>());
            painter_.setSize(aseWindow.getSize());
        }
        resized_ = false;

        auto frameId = getCurrentFrameId();


        auto timeToNext = titleSignal_.updateBegin({frameId, dt});

        auto timeToNext2 = widget_.update({frameId, dt});
        timeToNext = signal::min(timeToNext, timeToNext2);

        timeToNext2 = titleSignal_.updateEnd({frameId, dt});
        timeToNext = signal::min(timeToNext, timeToNext2);

        if (widget_.getInputAreas().hasChanged())
        {
            // If there's an area with the same id -> update
            auto areas = widget_.getInputAreas().evaluate();
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
        }

        if (titleSignal_.hasChanged())
            aseWindow.setTitle(titleSignal_.evaluate());

        if (redraw_ || widget_.getDrawing().hasChanged())
        {
            aseWindow.clear();

            ase::CommandBuffer commands;

            render(memory_, commands, context_,
                    aseWindow.getDefaultFramebuffer(),
                    aseWindow.getSize(), aseWindow.getScalingFactor(),
                    painter_, widget_.getDrawing().evaluate());

            context_.submit(std::move(commands));
            context_.present(aseWindow);
            redraw_ = false;

            ++frames_;
        }

        if (widget_.getKeyboardInputs().hasChanged())
        {
            auto inputs = widget_.getKeyboardInputs().evaluate();
            for (auto&& input : inputs)
            {
                auto handle = input.getFocusHandle();

                if (input.getRequestFocus() && handle.valid())
                {
                    if (currentHandle_.valid())
                        currentHandle_->set(false);
                    handle->set(true);
                    currentHandle_ = handle;
                    currentHandler_ = input.getHandler();
                    break;
                }
            }

            if (!currentHandle_.valid() && !inputs.empty())
            {
                auto handle = inputs[0];
                currentHandle_ = handle.getFocusHandle();
                if (currentHandle_.valid())
                    currentHandle_->set(true);
                currentHandler_ = handle.getHandler();
            }
        }

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

    Widget const& getWidget() const
    {
        return widget_;
    }

private:
    pmr::unsynchronized_pool_resource memoryPool_;
    pmr::statistics_resource memoryStatistics_;
    pmr::memory_resource* memory_;
    ase::Window aseWindow;
    ase::RenderContext context_;
    Window window_;
    avg::Painter painter_;
    signal::Input<ase::Vector2f> size_;
    Widget widget_;
    AnySignal<std::string> titleSignal_;
    //RenderCache cache_;
    bool resized_ = true;
    bool redraw_ = true;
    std::unordered_map<unsigned int, std::vector<InputArea>> areas_;
    std::unordered_map<ase::KeyCode,
        std::function<void(ase::KeyEvent const&)>> keys_;
    btl::option<signal::InputHandle<bool>> currentHandle_;
    btl::option<KeyboardInput::Handler> currentHandler_;
    uint64_t frames_ = 0;
    uint32_t pointerEventsOnThisFrame_ = 0;
    btl::option<InputArea> currentHoverArea_;
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

    std::vector<btl::shared<WindowGlue>> glues;
    glues.reserve(d()->windows_.size());

    for (auto&& w : d()->windows_)
    {
        ase::RenderContext context = platform.makeRenderContext();
        avg::Painter painter(context);

        glues.push_back(std::make_shared<WindowGlue>(
            platform, std::move(context), std::move(w), painter));
    }

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;

    DBG("Reactive running...");

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

        for (auto& glue : glues)
        {
            auto t = glue->frame(/*events,*/ dt);

            timeToNext = reactive::signal::min(timeToNext, t);

            /*if (glue->getWidget().getInputAreas().hasChanged())
            {
                std::cout << "Wrote test1.dot" << std::endl;
                auto s = glues.front()->getWidget()
                    .getInputAreas().annotate().getDot();
                std::ofstream fs("test1.dot");
                fs << s << std::endl;
                fs.close();
            }*/
        }

        if (timeToNext.valid())
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

    for (auto const& glue : glues)
    {
        DBG("Window \"%1\" had FPS of %2.", glue->getTitle(),
                (double)glue->getFrames() / time.count());
    }

    return 0;
}

int App::run() &&
{
    auto running = signal::input(true);
    for (auto&& w : d()->windows_)
        w = std::move(w).onClose(send(false, running.handle));

    return std::move(*this).run(running.signal);
}

} // namespace reactive

