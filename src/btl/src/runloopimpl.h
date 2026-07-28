#pragma once

#include <btl/runloop.h>

#include <functional>
#include <memory>
#include <thread>

namespace btl
{
    // The platform-agnostic surface RunLoop forwards to. One concrete subclass
    // per platform (posix/win32/...), built by makePlatformSpecificRunLoopImpl().
    class RunLoopImpl : public std::enable_shared_from_this<RunLoopImpl>
    {
    public:
        using Callback = std::function<void(RunLoop::Controller&)>;

        virtual ~RunLoopImpl() = default;

        virtual SourceId addReadable(NativeHandle, Callback) = 0;
        virtual SourceId addWritable(NativeHandle, Callback) = 0;
        virtual TimerId addTimer(std::chrono::microseconds, Callback) = 0;
        virtual void post(Callback) = 0;
        virtual void remove(SourceId) = 0;
        virtual void cancel(TimerId) = 0;
        virtual void run() = 0;
        virtual void stop() = 0;

        // Remove/cancel from a handle's destructor, which may run on any thread:
        // apply now on the loop thread, otherwise post it.
        void requestRemove(SourceId id)
        {
            onLoopThread([this, id] { remove(id); });
        }

        void requestCancel(TimerId id)
        {
            onLoopThread([this, id] { cancel(id); });
        }

    protected:
        void invoke(Callback const& callback)
        {
            RunLoop::Controller controller(this);
            callback(controller);
        }

        void enterLoopThread()
        {
            loopThread_ = std::this_thread::get_id();
        }

    private:
        void onLoopThread(std::function<void()> op)
        {
            if (std::this_thread::get_id() == loopThread_)
                op();
            else
                post([op = std::move(op)](RunLoop::Controller&) { op(); });
        }

        std::thread::id loopThread_;
    };

    std::shared_ptr<RunLoopImpl> makePlatformSpecificRunLoopImpl();
}
