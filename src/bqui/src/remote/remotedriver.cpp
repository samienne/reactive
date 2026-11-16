#include "bqui/remote/remotedriver.h"
#include "bqui/remote/transport.h"

#include "blobstore.h"
#include "capture.h"
#include "debug.h"
#include "dispatcher.h"
#include "introspectionjson.h"
#include "png.h"
#include "screenshot.h"

#include <avg/rect.h>
#include <avg/rendertree/snapshot.h>

#include <btl/runloop.h>
#include <btl/threadpool.h>

#include <nlohmann/json.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bqui::remote
{

using nlohmann::json;

RemoteWindow::~RemoteWindow() = default;

bool RemoteWindow::canCapture() const
{
    return false;
}

void RemoteWindow::captureNextFrame(ase::CaptureCallback callback)
{
    callback(ase::ReadbackError::Unsupported);
}

void RemoteWindow::requestFrame()
{
}

float RemoteWindow::scalingFactor() const
{
    return 1.0f;
}
>>>


namespace
{

// One nominal frame at ~60 Hz — the default step when a client gives no dt.
constexpr int64_t kNominalFrameUs = 16667;

// How long an orderly shutdown waits for the client to take more of a queued
// reply before giving up on it.
constexpr std::chrono::seconds kShutdownFlushTimeout{ 2 };

// --- Field readers: type-checked, defaulting rather than throwing ---------

double numberField(json const& obj, char const* key, double fallback)
{
    auto it = obj.find(key);
    if (it != obj.end() && it->is_number())
        return it->get<double>();
    return fallback;
}

std::string stringField(json const& obj, char const* key)
{
    auto it = obj.find(key);
    if (it != obj.end() && it->is_string())
        return it->get<std::string>();
    return std::string();
}

bool boolField(json const& obj, char const* key, bool fallback)
{
    auto it = obj.find(key);
    if (it != obj.end() && it->is_boolean())
        return it->get<bool>();
    return fallback;
}

// --- Injection: same event shapes as the interim protocol -----------------

void applyInjection(RemoteWindow& window, json const& event)
{
    if (!event.is_object())
        return;

    auto kind = stringField(event, "kind");
    ase::Vector2f pos(
            static_cast<float>(numberField(event, "x", 0.0)),
            static_cast<float>(numberField(event, "y", 0.0)));
    auto pointer = static_cast<unsigned int>(numberField(event, "pointer", 0.0));

    if (kind == "pointerButton")
    {
        auto state = stringField(event, "state");
        window.injectPointerButton(pointer,
                static_cast<unsigned int>(numberField(event, "button", 1.0)),
                pos,
                state == "up" ? ase::ButtonState::up : ase::ButtonState::down);
    }
    else if (kind == "pointerMove")
    {
        window.injectPointerMove(pointer, pos);
    }
    else if (kind == "hover")
    {
        window.injectHover(pointer, pos, boolField(event, "state", true));
    }
    else if (kind == "key")
    {
        auto state = stringField(event, "state");
        window.injectKey(
                state == "up" ? ase::KeyState::up : ase::KeyState::down,
                static_cast<ase::KeyCode>(
                    static_cast<unsigned int>(numberField(event, "code", 0.0))),
                static_cast<uint32_t>(numberField(event, "mods", 0.0)),
                stringField(event, "text"));
    }
    else if (kind == "text")
    {
        window.injectText(stringField(event, "text"));
    }
}

/** @brief Reject an injection event that is malformed or would escape the seam.
 *
 * An unknown `kind` would be silently dropped by applyInjection, so reject it.
 * The seam indexes fixed 15-slot arrays: pointer id must be `<= 15` and button
 * `1..15` (a `0` button underflows into an out-of-bounds write). Throws RpcError
 * naming the first offending field.
 */
void validateInjection(json const& event)
{
    if (!event.is_object())
        throw RpcError{ kInvalidParams, "each inject event must be an object" };

    auto kind = stringField(event, "kind");

    bool const isPointer = kind == "pointerButton" || kind == "pointerMove"
        || kind == "hover";

    bool const isKnown = isPointer || kind == "key" || kind == "text";
    if (!isKnown)
        throw RpcError{ kInvalidParams,
            "each inject event needs a known 'kind' (one of pointerButton, "
            "pointerMove, hover, key, text), got '" + kind + "'" };

    if (isPointer)
    {
        double pointer = numberField(event, "pointer", 0.0);
        if (pointer < 0.0 || pointer > 15.0)
            throw RpcError{ kInvalidParams,
                "'pointer' must be in [0, 15], got "
                    + std::to_string(pointer) };
    }

    if (kind == "pointerButton")
    {
        double button = numberField(event, "button", 1.0);
        if (button < 1.0 || button > 15.0)
            throw RpcError{ kInvalidParams,
                "'button' must be in [1, 15], got "
                    + std::to_string(button) };
    }
}

RemoteWindow* findWindow(RemoteWindows const& windows, uint64_t id)
{
    for (auto& window : windows)
        if (window.get().id().getValue() == id)
            return &window.get();

    return nullptr;
}

uint64_t requireWindowId(json const& params)
{
    auto it = params.find("window");
    if (it == params.end() || !it->is_number())
        throw RpcError{ kInvalidParams,
            "'window' must be a numeric window id" };

    return it->get<uint64_t>();
}

// --- Screenshots -----------------------------------------------------------

struct ScreenshotRequest
{
    uint64_t window = 0;
    std::optional<LogicalRect> region;
    std::optional<uint64_t> widget;
    float scale = 1.0f;
};

bool isFiniteNumber(json const& value)
{
    return value.is_number() && std::isfinite(value.get<double>());
}

LogicalRect parseRegion(json const& region)
{
    if (!region.is_object())
        throw RpcError{ kInvalidParams,
            "'region' must be an object with x, y, width and height" };

    auto field = [&](char const* key)
    {
        auto it = region.find(key);
        if (it == region.end() || !isFiniteNumber(*it))
            throw RpcError{ kInvalidParams,
                std::string("'region.") + key + "' must be a finite number" };
        return static_cast<float>(it->get<double>());
    };

    LogicalRect rect{ field("x"), field("y"), field("width"),
        field("height") };
    if (!(rect.width > 0.0f) || !(rect.height > 0.0f))
        throw RpcError{ kInvalidParams,
            "'region' must have a positive width and height" };

    return rect;
}

ScreenshotRequest parseScreenshot(json const& params)
{
    ScreenshotRequest request;
    request.window = requireWindowId(params);

    auto region = params.find("region");
    auto widget = params.find("widget");
    if (region != params.end() && widget != params.end())
        throw RpcError{ kInvalidParams,
            "give 'region' or 'widget', not both" };

    if (region != params.end())
        request.region = parseRegion(*region);

    if (widget != params.end())
    {
        if (!widget->is_number_unsigned())
            throw RpcError{ kInvalidParams,
                "'widget' must be an introspection id" };
        request.widget = widget->get<uint64_t>();
    }

    auto scale = params.find("scale");
    if (scale != params.end())
    {
        if (!isFiniteNumber(*scale) || scale->get<double>() <= 0.0
                || scale->get<double>() > 1.0)
            throw RpcError{ kInvalidParams, "'scale' must be in (0, 1]" };
        request.scale = static_cast<float>(scale->get<double>());
    }

    return request;
}

std::optional<LogicalRect> widgetBounds(widget::Introspection const& node,
        uint64_t id)
{
    if (node.id && node.id->getValue() == id)
    {
        auto rect = node.obb.getBoundingRect();
        return LogicalRect{ rect.getLeft(), rect.getBottom(), rect.getWidth(),
            rect.getHeight() };
    }

    for (auto const& child : node.children)
        if (child)
            if (auto bounds = widgetBounds(*child, id))
                return bounds;

    return std::nullopt;
}

json toJson(LogicalRect const& rect)
{
    return {
        { "x", rect.x },
        { "y", rect.y },
        { "width", rect.width },
        { "height", rect.height },
    };
}

RpcError captureError(ase::ReadbackError error, uint64_t window)
{
    json data = { { "window", window } };
    std::string const name = "window " + std::to_string(window);

    switch (error)
    {
    case ase::ReadbackError::Unsupported:
        data["reason"] = "unsupported";
        return { kCaptureUnsupported,
            name + " cannot capture: its backend cannot read pixels back",
            data };
    case ase::ReadbackError::Cancelled:
        data["reason"] = "cancelled";
        return { kInternalError,
            name + " went away before a frame was captured", data };
    case ase::ReadbackError::Failed:
        break;
    }

    data["reason"] = "failed";
    return { kInternalError, name + " failed to read its frame back", data };
}

} // namespace

/** @brief The inspector-protocol server behind RemoteDriver.
 *
 * Launches paused. Registers the transport as a readable source on the injected
 * loop and, while running, advances the app off a loop timer. Single-threaded:
 * only the loop thread touches the app, so there is no queue or reader thread.
 * It does not own the loop; the caller runs it.
 */
class RemoteDriver::Impl
{
public:
    Impl(btl::RunLoop& loop, Transport& transport, RemoteApp app,
            std::optional<ase::PauseToken> pauseToken,
            std::chrono::milliseconds stepTimeout, BlobLimits blobLimits) :
        loop_(loop),
        transport_(&transport),
        app_(std::move(app)),
        pauseToken_(std::move(pauseToken)),
        stepTimeout_(stepTimeout),
        blobs_(blobLimits),
        dispatcher_([this](std::string const& frame) { safeSend(frame); })
    {
#ifdef SIGPIPE
        // Linux has no per-socket SIGPIPE suppression, so ignore it
        // process-wide; otherwise a client disconnecting mid-write kills the
        // app.
        std::signal(SIGPIPE, SIG_IGN);
#endif
        buildRegistry();

        if (pauseToken_)
        {
            pauseToken_->setWakeCallback([this]
                {
                    if (pendingStep_ || zeroFrameGate_)
                        post([this]
                            {
                                resumeStep();
                                resumeZeroFrame();
                            });
                });
        }

        loop_.post([this](btl::RunLoop::Controller& controller)
        {
            ControllerScope scope(controller_, controller);

            readable_ = controller.addReadable(transport_->nativeHandle(),
                    [this](btl::RunLoop::Controller& c) { onReadable(c); });

            // A completion-based backend (Win32 named pipe) only signals
            // readable once an overlapped read is pending, so this first
            // receiveReady() kicks that read off (a no-op for socket backends).
            receive();
        });
    }

    ~Impl()
    {
        endSession();

        std::unique_lock<std::mutex> lock(encodes_->mutex);
        encodes_->done.wait(lock, [this] { return encodes_->running == 0; });
    }

private:
    enum class State { Paused, Running };

    // An `app.step` whose frames are not all rendered yet.
    struct PendingStep
    {
        int64_t remaining;
        std::chrono::microseconds dt;
        Responder responder;
        DispatchGate gate;
    };

    // A `window.screenshot` not answered yet; `framed` once its frame is
    // captured and being encoded.
    struct PendingScreenshot
    {
        ScreenshotRequest request;
        float scalingFactor = 1.0f;
        Responder responder;
        btl::RunLoop::Timer deadline;
        bool framed = false;
    };

    // The encodes running on worker threads, which the destructor waits for
    // so that none posts to a loop being torn down.
    struct EncodeJobs
    {
        std::mutex mutex;
        std::condition_variable done;
        int running = 0;
    };

    // The loop-thread Controller, valid only during a callback.
    struct ControllerScope
    {
        btl::RunLoop::Controller*& slot;
        btl::RunLoop::Controller* prev;
        ControllerScope(btl::RunLoop::Controller*& s, btl::RunLoop::Controller& c)
            : slot(s), prev(s) { slot = &c; }
        ~ControllerScope() { slot = prev; }
    };

    void onReadable(btl::RunLoop::Controller& controller)
    {
        ControllerScope scope(controller_, controller);
        receive();
    }

    // Runs `task` in a loop callback of its own, unless the driver is gone by
    // then.
    void post(std::function<void()> task)
    {
        loop_.post([this, alive = std::weak_ptr<bool>(alive_),
                task = std::move(task)](btl::RunLoop::Controller& controller)
            {
                if (alive.expired())
                    return;

                ControllerScope scope(controller_, controller);
                task();
            });
    }

    // Keeps draining while shutting down, so a readable channel does not spin
    // the loop while the last reply flushes; those frames are discarded.
    void receive()
    {
        for (auto& frame : transport_->receiveReady())
        {
            if (shuttingDown_)
                break;

            dispatcher_.dispatch(std::move(frame));
        }

        // A peer that only half-closed still reads, so the queue keeps
        // flushing; the drained channel would report readable for good.
        if (transport_->disconnected())
            readable_ = btl::RunLoop::Source();

        // A step still waiting on its frame answers once it renders, as do the
        // requests queued behind it; the step deadline bounds that wait.
        if (shuttingDown_)
            finish();
        else if (transport_->disconnected())
        {
            if (!idle() && !closed_)
                inputEnded_ = true;
            else
                finish();
        }
    }

    void safeSend(std::string const& frame)
    {
        if (closed_)
            return;

        try
        {
            transport_->enqueue(frame);
        }
        catch (std::exception const& e)
        {
            close(e.what());
            return;
        }

        assert(controller_);
        flushQueued();
    }

    void flushQueued()
    {
        if (closed_)
            return;

        try
        {
            flush();
        }
        catch (std::exception const& e)
        {
            close(e.what());
        }
    }

    void flush()
    {
        if (transport_->flushReady())
        {
            writable_ = btl::RunLoop::Source();
            return;
        }

        if (writable_.id() == 0)
        {
            writable_ = controller_->addWritable(transport_->writableHandle(),
                    [this](btl::RunLoop::Controller& c) { onWritable(c); });
        }
    }

    void onWritable(btl::RunLoop::Controller& controller)
    {
        ControllerScope scope(controller_, controller);

        flushQueued();

        if (closed_ || !shuttingDown_)
            return;

        if (writable_.id() == 0)
            stop();
        else
            armFlushTimeout();
    }

    void beginShutdown()
    {
        shuttingDown_ = true;
        dispatcher_.stop();
    }

    void endSession()
    {
        shuttingDown_ = true;
        dispatcher_.end();
        pendingStep_.reset();
        stepDeadline_ = btl::RunLoop::Timer();
        screenshots_.clear();
        zeroFrameGate_.reset();
        zeroFrameDeadline_ = btl::RunLoop::Timer();
        blobs_.clear();
    }

    void close(char const* reason)
    {
        DBG("Remote connection closed: %1", reason);
        closed_ = true;
        endSession();
        stop();
    }

    void finish()
    {
        endSession();

        if (writable_.id() == 0)
            stop();
        else if (flushTimeout_.id() == 0)
            armFlushTimeout();
    }

    void armFlushTimeout()
    {
        flushTimeout_ = controller_->addTimer(kShutdownFlushTimeout,
                [this](btl::RunLoop::Controller&)
                {
                    close("the client did not read the last reply");
                });
    }

    void stop()
    {
        writable_ = btl::RunLoop::Source();
        flushTimeout_ = btl::RunLoop::Timer();
        loop_.stop();
    }

    void ensureTicking()
    {
        if (ticking_ || shuttingDown_ || state_ != State::Running)
            return;

        ticking_ = true;
        controller_->addTimer(std::chrono::microseconds(kNominalFrameUs),
                [this](btl::RunLoop::Controller& c) { onTick(c); }).detach();
    }

    void onTick(btl::RunLoop::Controller& controller)
    {
        ControllerScope scope(controller_, controller);
        ticking_ = false;

        if (shuttingDown_)
        {
            finish();
            return;
        }

        if (state_ != State::Running)
            return;

        advanceFrame(std::chrono::microseconds(kNominalFrameUs));
        if (shuttingDown_)
        {
            finish();
            return;
        }

        if (frameNotifications_)
            sendFrameNotification(std::chrono::microseconds(kNominalFrameUs));

        ensureTicking();
    }

    void advanceFrame(std::chrono::microseconds dt)
    {
        bool keepRunning = pauseToken_ ? pauseToken_->step(dt) : app_.step(dt);
        ++frame_;
        if (!keepRunning)
            beginShutdown();
    }

    void sendFrameNotification(std::chrono::microseconds dt)
    {
        json note = {
            { "jsonrpc", "2.0" },
            { "method", "frame" },
            { "params", {
                { "index", frame_ },
                { "dt_us", dt.count() },
            } },
        };
        safeSend(note.dump());
    }

    // --- Handlers ---------------------------------------------------------

    json describe(json const&) const
    {
        json methods = json::array();
        for (auto const& method : dispatcher_.methods())
        {
            json params = json::array();
            for (auto const& param : method.params)
                params.push_back({
                        { "name", param.name },
                        { "type", param.type },
                        { "required", param.required },
                        });

            methods.push_back({
                    { "name", method.name },
                    { "doc", method.doc },
                    { "params", params },
                    { "returnsBlob", method.returnsBlob },
                    });
        }

        return {
            { "methods", methods },
            { "limits", { { "blob", blobs_.describeLimits() } } },
        };
    }

    json run(json const&)
    {
        state_ = State::Running;
        ensureTicking();
        return { { "state", "running" } };
    }

    json pause(json const&)
    {
        state_ = State::Paused;

        for (auto const& entry : screenshots_)
        {
            if (!entry.second.framed)
            {
                renderZeroFrame();
                break;
            }
        }

        return { { "state", "paused" }, { "frame", frame_ } };
    }

    Reply step(json const& params, Responder& responder)
    {
        auto count = static_cast<int64_t>(numberField(params, "count", 1.0));
        if (count < 0)
            count = 0;

        auto dt = std::chrono::microseconds(static_cast<int64_t>(
                numberField(params, "dt_us",
                    static_cast<double>(kNominalFrameUs))));

        // A step always leaves the app paused, whichever state it started from.
        state_ = State::Paused;

        if (advanceSteps(count, dt))
            return stepResult();

        pendingStep_.emplace(PendingStep{ count, dt, std::move(responder),
                dispatcher_.holdGate() });
        armStepDeadline();
        return pending;
    }

    // Advances `remaining` frames, each only once the one before it has
    // rendered every window it was due in. Reports false when it has to stop
    // for a window with no free in-flight slot.
    bool advanceSteps(int64_t& remaining, std::chrono::microseconds dt)
    {
        for (;;)
        {
            if (shuttingDown_)
                return true;

            if (pauseToken_ && !pauseToken_->finishStep())
                return false;

            if (remaining <= 0)
                return true;

            --remaining;
            advanceFrame(dt);
        }
    }

    void resumeStep()
    {
        if (!pendingStep_)
            return;

        int64_t before = pendingStep_->remaining;
        if (!advanceSteps(pendingStep_->remaining, pendingStep_->dt))
        {
            if (pendingStep_->remaining != before)
                armStepDeadline();
            return;
        }

        endStep([this](Responder& responder)
            {
                responder.resolve(stepResult());
            });
    }

    void armStepDeadline()
    {
        stepDeadline_ = controller_->addTimer(stepTimeout_,
                [this](btl::RunLoop::Controller& controller)
                {
                    ControllerScope scope(controller_, controller);
                    endStep([](Responder& responder)
                        {
                            responder.reject(RpcError{ kInternalError,
                                "the frame did not finish rendering" });
                        });
                });
    }

    // Answers the pending step and lets the requests behind it go ahead.
    void endStep(std::function<void(Responder&)> const& answer)
    {
        PendingStep step = std::move(*pendingStep_);
        pendingStep_.reset();
        stepDeadline_ = btl::RunLoop::Timer();

        answer(step.responder);
        step.gate.release();

        if (shuttingDown_ || (inputEnded_ && idle()))
            finish();
    }

    // Whether nothing is left to answer once input has ended.
    bool idle() const
    {
        return !pendingStep_ && !zeroFrameGate_ && screenshots_.empty();
    }

    void finishIfIdle()
    {
        if (inputEnded_ && !shuttingDown_ && idle())
            finish();
    }

    json stepResult() const
    {
        return { { "state", "paused" }, { "frame", frame_ } };
    }

    Reply windowScreenshot(json const& params, Responder& responder)
    {
        ScreenshotRequest request = parseScreenshot(params);

        auto windows = app_.liveWindows();
        RemoteWindow* window = findWindow(windows, request.window);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(request.window) };

        if (request.widget
                && !widgetBounds(window->introspect(), *request.widget))
            throw RpcError{ kInvalidParams,
                "no widget with id " + std::to_string(*request.widget)
                    + " in window " + std::to_string(request.window) };

        if (!window->canCapture())
            throw captureError(ase::ReadbackError::Unsupported,
                    request.window);

        if (screenshots_.size() >= kMaxScreenshotsInFlight)
            throw RpcError{ kCaptureBusy, "too many screenshots in flight",
                { { "inFlight", screenshots_.size() },
                  { "limit", kMaxScreenshotsInFlight } } };

        uint64_t const key = nextScreenshot_++;
        PendingScreenshot& shot = screenshots_[key];
        shot.request = request;
        shot.scalingFactor = window->scalingFactor();
        shot.responder = std::move(responder);
        shot.deadline = controller_->addTimer(stepTimeout_,
                [this, key](btl::RunLoop::Controller& controller)
                {
                    ControllerScope scope(controller_, controller);
                    failScreenshot(key, RpcError{ kInternalError,
                        "no frame was captured in time" });
                });

        captureOnLoop(*window, loop_, alive_,
                [this, key](btl::RunLoop::Controller& controller,
                    ase::CaptureResult result)
                {
                    ControllerScope scope(controller_, controller);
                    onCaptured(key, std::move(result));
                });
        window->requestFrame();

        if (state_ == State::Paused)
            renderZeroFrame();

        return pending;
    }

    // Renders the due windows without moving time or the frame index, so a
    // paused app serves an armed capture. Holds the gate while a window waits
    // for a free slot.
    void renderZeroFrame()
    {
        auto const dt = std::chrono::microseconds(0);
        bool keepRunning = pauseToken_ ? pauseToken_->step(dt) : app_.step(dt);
        if (!keepRunning)
        {
            beginShutdown();
            return;
        }

        if (pauseToken_ && !pauseToken_->finishStep() && !zeroFrameGate_)
        {
            zeroFrameGate_ = dispatcher_.holdGate();
            zeroFrameDeadline_ = controller_->addTimer(stepTimeout_,
                    [this](btl::RunLoop::Controller& controller)
                    {
                        ControllerScope scope(controller_, controller);
                        endZeroFrame();
                    });
        }
    }

    void resumeZeroFrame()
    {
        if (zeroFrameGate_ && pauseToken_->finishStep())
            endZeroFrame();
    }

    void endZeroFrame()
    {
        if (!zeroFrameGate_)
            return;

        zeroFrameDeadline_ = btl::RunLoop::Timer();
        DispatchGate gate = std::move(*zeroFrameGate_);
        zeroFrameGate_.reset();
        gate.release();

        finishIfIdle();
    }

    void onCaptured(uint64_t key, ase::CaptureResult result)
    {
        auto it = screenshots_.find(key);
        if (it == screenshots_.end())
            return;

        PendingScreenshot& shot = it->second;
        shot.deadline = btl::RunLoop::Timer();

        if (!result.valid())
        {
            failScreenshot(key, captureError(result.err(),
                        shot.request.window));
            return;
        }

        std::optional<LogicalRect> region = shot.request.region;
        if (shot.request.widget)
        {
            auto windows = app_.liveWindows();
            RemoteWindow* window = findWindow(windows, shot.request.window);
            region = window
                ? widgetBounds(window->introspect(), *shot.request.widget)
                : std::nullopt;
            if (!region)
            {
                failScreenshot(key, RpcError{ kInvalidParams,
                    "widget " + std::to_string(*shot.request.widget)
                        + " is not in the captured frame" });
                return;
            }
        }

        ase::FrameCapture capture = std::move(result).value();
        auto plan = planScreenshot(capture.image.size, shot.scalingFactor,
                region, shot.request.scale);
        if (!plan)
        {
            failScreenshot(key, RpcError{ kInvalidParams,
                "the region lies outside the window" });
            return;
        }

        shot.framed = true;
        encode(key, std::move(capture), *plan);
    }

    void encode(uint64_t key, ase::FrameCapture capture, ScreenshotPlan plan)
    {
        {
            std::lock_guard<std::mutex> lock(encodes_->mutex);
            ++encodes_->running;
        }

        btl::ThreadPool::getInstance().async(
                [this, key, plan, jobs = encodes_,
                alive = std::weak_ptr<bool>(alive_),
                capture = std::move(capture)]() mutable
                {
                    std::string png;
                    std::string error;
                    try
                    {
                        auto rgb = renderScreenshot(capture.image, plan);
                        png = encodePng(
                                static_cast<uint32_t>(plan.outputSize.x()),
                                static_cast<uint32_t>(plan.outputSize.y()),
                                rgb.data());
                    }
                    catch (std::exception const& e)
                    {
                        error = e.what();
                    }

                    loop_.post([this, key, plan, alive,
                            presentIndex = capture.presentIndex,
                            png = std::move(png), error = std::move(error)](
                                btl::RunLoop::Controller& controller) mutable
                        {
                            if (alive.expired())
                                return;

                            ControllerScope scope(controller_, controller);
                            onEncoded(key, plan, presentIndex, std::move(png),
                                    error);
                        });

                    {
                        std::lock_guard<std::mutex> lock(jobs->mutex);
                        --jobs->running;
                    }
                    jobs->done.notify_all();
                });
    }

    void onEncoded(uint64_t key, ScreenshotPlan const& plan,
            uint64_t presentIndex, std::string png, std::string const& error)
    {
        auto it = screenshots_.find(key);
        if (it == screenshots_.end())
            return;

        PendingScreenshot shot = std::move(it->second);
        screenshots_.erase(it);

        if (!error.empty())
        {
            shot.responder.reject(RpcError{ kInternalError,
                "encoding the screenshot failed: " + error });
            finishIfIdle();
            return;
        }

        json blob;
        try
        {
            blob = blobs_.add(std::move(png), "image/png");
        }
        catch (RpcError const& e)
        {
            shot.responder.reject(e);
            finishIfIdle();
            return;
        }

        shot.responder.resolve({
                { "blob", std::move(blob) },
                { "width", plan.outputSize.x() },
                { "height", plan.outputSize.y() },
                { "region", toJson(plan.captured) },
>>>

                { "presentIndex", presentIndex },
                });
        finishIfIdle();
    }

    void failScreenshot(uint64_t key, RpcError error)
    {
        auto it = screenshots_.find(key);
        if (it == screenshots_.end())
            return;

        PendingScreenshot shot = std::move(it->second);
        screenshots_.erase(it);

        shot.responder.reject(std::move(error));
        finishIfIdle();
    }

    json setFrameNotifications(json const& params)
    {
        frameNotifications_ = boolField(params, "enabled", false);
        return json::object();
    }

    json shutdown(json const&)
    {
        beginShutdown();
        return json::object();
    }

    json windowList(json const&)
    {
        app_.sync();

        json windows = json::array();
        for (auto& window : app_.liveWindows())
            windows.push_back({
                    { "id", window.get().id().getValue() },
                    { "canCapture", window.get().canCapture() },
                    });

        return { { "windows", windows } };
    }

    json windowIntrospect(json const& params)
    {
        uint64_t id = requireWindowId(params);

        auto windows = app_.liveWindows();
        RemoteWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        return { { "introspection", toJson(window->introspect()) } };
    }

    json windowRenderTree(json const& params)
    {
        uint64_t id = requireWindowId(params);

        auto windows = app_.liveWindows();
        RemoteWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        auto asBlob = params.find("asBlob");
        if (asBlob != params.end() && !asBlob->is_boolean())
            throw RpcError{ kInvalidParams, "'asBlob' must be a boolean" };

        json result = { { "renderTree", toJson(window->snapshot()) } };
        if (asBlob == params.end() || !asBlob->get<bool>())
            return result;

        return { { "blob", blobs_.add(result.dump(), "application/json") } };
    }

    json windowInject(json const& params)
    {
        uint64_t id = requireWindowId(params);

        auto windows = app_.liveWindows();
        RemoteWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        auto events = params.find("events");
        if (events == params.end() || !events->is_array())
            throw RpcError{ kInvalidParams, "'events' must be an array" };

        // Validate the whole batch first so a malformed event rejects
        // atomically -- none of the batch is applied.
        for (auto const& event : *events)
            validateInjection(event);

        for (auto const& event : *events)
            applyInjection(*window, event);

        return json::object();
    }

    void buildRegistry()
    {
        dispatcher_.add({ "system.describe",
            "List every method, its doc, its parameter schema and whether it "
            "can answer with a blob, and the session's limits.",
            {},
            [this](json const& p, Responder&) { return describe(p); } });

        dispatcher_.add({ "app.run",
            "Enter running mode: free-run frames until paused or shut down.",
            {},
            [this](json const& p, Responder&) { return run(p); } });

        dispatcher_.add({ "app.pause",
            "Enter paused mode: hold the current frame until told otherwise.",
            {},
            [this](json const& p, Responder&) { return pause(p); } });

        dispatcher_.add({ "app.step",
            "Advance `count` frames by `dt_us` each, then stay paused.",
            {
                { "count", "number", false },
                { "dt_us", "number", false },
            },
            [this](json const& p, Responder& r) { return step(p, r); } });

        dispatcher_.add({ "app.shutdown",
            "End the session and let the app release its windows.",
            {},
            [this](json const& p, Responder&) { return shutdown(p); } });

        dispatcher_.add({ "app.setFrameNotifications",
            "Toggle per-frame `frame` notifications while running (off by "
            "default).",
            { { "enabled", "boolean", true } },
            [this](json const& p, Responder&)
            {
                return setFrameNotifications(p);
            } });

        dispatcher_.add({ "window.list",
            "List the live windows by id, with whether each can be "
            "captured (`canCapture`).",
            {},
            [this](json const& p, Responder&) { return windowList(p); } });

        dispatcher_.add({ "window.introspect",
            "The window's resolved widget (introspection) tree.",
            { { "window", "number", true } },
            [this](json const& p, Responder&)
            {
                return windowIntrospect(p);
            } });

        Method renderTree{ "window.renderTree",
            "The window's render-tree snapshot; with `asBlob`, a blob holding "
            "the same reply.",
            {
                { "window", "number", true },
                { "asBlob", "boolean", false },
            },
            [this](json const& p, Responder&)
            {
                return windowRenderTree(p);
            } };
        renderTree.returnsBlob = true;
        dispatcher_.add(std::move(renderTree));

        Method screenshot{ "window.screenshot",
            "Capture the window's next frame as a PNG blob; while paused, a "
            "frame with no elapsed time is rendered for it. `region` {x, y, "
            "width, height} in window-space units (x, y its bottom-left "
            "corner) or `widget` (an introspection id), not both, defaults to "
            "the whole window; `scale` in (0, 1] shrinks the image. Answers "
            "with the blob, the image `width` and `height` in pixels, the "
            "captured `region` in window-space units and the frame's "
            "`presentIndex`; `region` maps onto the whole image, so the two "
            "give the exact pixels per unit on each axis.",
            {
                { "window", "number", true },
                { "region", "object", false },
                { "widget", "number", false },
                { "scale", "number", false },
            },
            [this](json const& p, Responder& r)
            {
                return windowScreenshot(p, r);
            } };
        screenshot.returnsBlob = true;
        dispatcher_.add(std::move(screenshot));

        dispatcher_.add({ "window.inject",
            "Queue input events onto a window for the next step.",
            {
                { "window", "number", true },
                { "events", "array", true },
            },
            [this](json const& p, Responder&) { return windowInject(p); } });

        dispatcher_.add({ "blob.read",
            "Up to `length` bytes of a blob from `offset`, base64-encoded.",
            {
                { "id", "number", true },
                { "offset", "number", true },
                { "length", "number", false },
            },
            [this](json const& p, Responder&) { return blobs_.read(p); } });

        dispatcher_.add({ "blob.release",
            "Free a blob; its id is unknown from then on.",
            { { "id", "number", true } },
            [this](json const& p, Responder&) { return blobs_.release(p); } });
    }

    btl::RunLoop& loop_;
    btl::RunLoop::Controller* controller_ = nullptr;
    Transport* transport_ = nullptr;
    RemoteApp app_;
    std::optional<ase::PauseToken> pauseToken_;
    std::chrono::milliseconds stepTimeout_;
    BlobStore blobs_;
    Dispatcher dispatcher_;
    std::optional<PendingStep> pendingStep_;
    std::map<uint64_t, PendingScreenshot> screenshots_;
    uint64_t nextScreenshot_ = 1;
    std::optional<DispatchGate> zeroFrameGate_;
    std::shared_ptr<EncodeJobs> encodes_ = std::make_shared<EncodeJobs>();
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    uint64_t frame_ = 0;
    State state_ = State::Paused;
    bool shuttingDown_ = false;
    bool inputEnded_ = false;
    bool closed_ = false;
    bool frameNotifications_ = false;
    bool ticking_ = false;
    btl::RunLoop::Source readable_;
    btl::RunLoop::Source writable_;
    btl::RunLoop::Timer flushTimeout_;
    btl::RunLoop::Timer stepDeadline_;
    btl::RunLoop::Timer zeroFrameDeadline_;
};

RemoteDriver::RemoteDriver(btl::RunLoop& loop, Transport& transport,
        RemoteApp app, std::optional<ase::PauseToken> pauseToken,
        std::chrono::milliseconds stepTimeout, BlobLimits blobLimits) :
    impl_(std::make_unique<Impl>(loop, transport, std::move(app),
                std::move(pauseToken), stepTimeout, blobLimits))
{
}

RemoteDriver::~RemoteDriver() = default;

} // namespace bqui::remote
