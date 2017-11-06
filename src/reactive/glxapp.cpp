#include "glxapp.h"

#include "rendering.h"

#include "debug.h"

#include "signal/input.h"
#include "send.h"

#include <ase/keyevent.h>
#include <ase/pointerbuttonevent.h>
#include <ase/rendercontext.h>
#include <ase/glxwindow.h>
#include <ase/glxplatform.h>

#include <chrono>
#include <fstream>

namespace reactive
{

namespace
{
    uint64_t s_frameId(0);

    uint64_t getNextFrameId()
    {
        return ++s_frameId;
    }

    uint64_t getCurrentFrameId()
    {
        return s_frameId;
    }
} // anonymous namespace

GlxApp::GlxApp()
{
}

void GlxApp::addWindows(std::vector<Window> windows)
{
    for (auto&& w : windows)
        windows_.push_back(std::move(w));
}

class GlxWindowGlue
{
public:
    GlxWindowGlue(ase::GlxPlatform& platform, ase::RenderContext&& context,
            Window window, avg::Painter painter) :
        glxWindow(platform, ase::Vector2i(800, 600)),
        context_(std::move(context)),
        window_(std::move(window)),
        painter_(std::move(painter)),
        size_(signal::input(ase::Vector2f(800, 600))),
        widget_(window_.getWidget()(std::move(size_.signal))),
        titleSignal_(window_.getTitle().clone())
    {
        glxWindow.setVisible(true);
        glxWindow.setTitle(titleSignal_.evaluate());

        glxWindow.setCloseCallback([this]()
            { window_.invokeOnClose(); });
        glxWindow.setResizeCallback([this]()
            { resized_ = true; });
        glxWindow.setRedrawCallback([this]()
            { redraw_ = true; });

        glxWindow.setButtonCallback([this](ase::PointerButtonEvent const& e)
            {
                if (pointerEventsOnThisFrame_++ > 0)
                {
                    widget_.update({getNextFrameId(), signal_time_t(0)});
                }

                if (e.getState() == ase::ButtonState::down)
                {
                    for (auto const& a : widget_.getAreas().evaluate())
                    {
                        if (a.contains(e.getPos()))
                        {
                            a.emit(e);
                            areas_.insert(std::make_pair(e.getButton(), a));
                        }
                    }
                }
                else if (e.getState() == ase::ButtonState::up)
                {
                    auto i = areas_.find(e.getButton());
                    while (i != areas_.end())
                    {
                        i->second.emit(e);
                        areas_.erase(i++);
                    }
                }
            });

        glxWindow.setKeyCallback([this](ase::KeyEvent const& e)
            {
                if (currentHandler_.valid() && e.isDown())
                {
                    (*currentHandler_)(e);
                    keys_[e.getKey()] = *currentHandler_;
                }
                else
                {
                    auto i = keys_.find(e.getKey());
                    if (i == keys_.end())
                        return;
                    auto f = i->second;
                    keys_.erase(i);
                    f(e);
                }
            });
    }

    GlxWindowGlue(GlxWindowGlue const&) = delete;
    GlxWindowGlue& operator=(GlxWindowGlue const&) = delete;

    virtual ~GlxWindowGlue()
    {
    }

    btl::option<signal_time_t> frame(std::vector<XEvent> const& events,
            std::chrono::microseconds dt)
    {
        pointerEventsOnThisFrame_ = 0;
        glxWindow.handleEvents(events);

        if (resized_)
            size_.handle.set(glxWindow.getSize().cast<float>());
        resized_ = false;

        auto frameId = getCurrentFrameId();


        auto timeToNext = titleSignal_.updateBegin({frameId, dt});

        auto timeToNext2 = widget_.update({frameId, dt});
        timeToNext = signal::min(timeToNext, timeToNext2);

        timeToNext2 = titleSignal_.updateEnd({frameId, dt});
        timeToNext = signal::min(timeToNext, timeToNext2);

        if (titleSignal_.hasChanged())
            glxWindow.setTitle(titleSignal_.evaluate());

        if (redraw_ || widget_.getDrawing().hasChanged())
        {
            glxWindow.clear();
            cache_ = render(context_, cache_, glxWindow, painter_,
                    widget_.getDrawing().evaluate());
            glxWindow.submitAll(context_);
            context_.present(glxWindow);
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
    ase::GlxWindow glxWindow;
    ase::RenderContext context_;
    Window window_;
    avg::Painter painter_;
    signal::Input<ase::Vector2f> size_;
    Widget widget_;
    Signal<std::string> titleSignal_;
    RenderCache cache_;
    bool resized_ = true;
    bool redraw_ = true;
    std::unordered_multimap<unsigned int, InputArea> areas_;
    std::unordered_map<ase::KeyCode,
        std::function<void(ase::KeyEvent const&)>> keys_;
    btl::option<signal::InputHandle<bool>> currentHandle_;
    btl::option<KeyboardInput::Handler> currentHandler_;
    uint64_t frames_ = 0;
    uint32_t pointerEventsOnThisFrame_ = 0;
};

int GlxApp::run() &&
{
    auto running = signal::input(true);
    for (auto&& w : windows_)
        w = std::move(w).onClose(send(false, running.handle));

    return std::move(*this).run(std::move(running.signal));
}

int GlxApp::run(Signal<bool> running) &&
{
    ase::GlxPlatform platform;
    ase::RenderContext& bgContext = platform.getDefaultContext();

    avg::Painter painter(bgContext);

    std::vector<btl::shared<GlxWindowGlue>> glues;
    glues.reserve(windows_.size());

    for (auto&& w : windows_)
    {
        glues.push_back(std::make_shared<GlxWindowGlue>(platform,
                    ase::RenderContext(platform), std::move(w), painter));
    }

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;

    DBG("Reactive Glx backend running...");

    while (running.evaluate())
    {
        auto thisFrame = clock.now();
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);

        auto events = platform.getEvents();

        reactive::signal::FrameInfo frame{getNextFrameId(), dt};
        auto timeToNext = running.updateBegin(frame);
        auto timeToNext2 = running.updateEnd(frame);
        timeToNext = reactive::signal::min(timeToNext, timeToNext2);

        for (auto& glue : glues)
        {
            auto t = glue->frame(events, dt);

            timeToNext = reactive::signal::min(timeToNext, t);

            /*if (glue->getWidget().getAreas().hasChanged())
            {
                std::cout << "Wrote test1.dot" << std::endl;
                auto s = glues.front()->getWidget()
                    .getAreas().annotate().getDot();
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
                usleep(remaining.count());
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

} // namespace

