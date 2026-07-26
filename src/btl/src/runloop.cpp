#include <btl/runloop.h>

#include "runloopimpl.h"

#include <utility>

namespace btl
{
    RunLoop::RunLoop() :
        impl_(makeRunLoopImpl())
    {
    }

    RunLoop::~RunLoop() = default;

    RunLoop::RunLoop(RunLoop&&) noexcept = default;
    RunLoop& RunLoop::operator=(RunLoop&&) noexcept = default;

    SourceId RunLoop::addReadable(NativeHandle handle,
            std::function<void()> onReadable)
    {
        return impl_->addReadable(handle, std::move(onReadable));
    }

    SourceId RunLoop::addWritable(NativeHandle handle,
            std::function<void()> onWritable)
    {
        return impl_->addWritable(handle, std::move(onWritable));
    }

    TimerId RunLoop::addTimer(std::chrono::microseconds delay,
            std::function<void()> callback)
    {
        return impl_->addTimer(delay, std::move(callback));
    }

    void RunLoop::post(std::function<void()> task)
    {
        impl_->post(std::move(task));
    }

    void RunLoop::remove(SourceId id)
    {
        impl_->remove(id);
    }

    void RunLoop::cancel(TimerId id)
    {
        impl_->cancel(id);
    }

    void RunLoop::run()
    {
        impl_->run();
    }

    void RunLoop::stop()
    {
        impl_->stop();
    }
}
