#include <btl/runloop.h>

#include "runloopimpl.h"

#include <atomic>
#include <stdexcept>
#include <utility>

namespace btl
{
    namespace
    {
        // The process default loop, or nullptr when none is registered. A bare
        // pointer, not an owner: a default loop registers itself here in its
        // constructor and deregisters in its destructor, so the pointer is valid
        // exactly while that loop lives.
        std::atomic<RunLoop*> g_defaultRunLoop{ nullptr };
    } // namespace

    RunLoop::RunLoop() :
        impl_(makePlatformSpecificRunLoopImpl())
    {
    }

    RunLoop::RunLoop(DefaultTag) :
        RunLoop()
    {
        RunLoop* expected = nullptr;
        if (!g_defaultRunLoop.compare_exchange_strong(expected, this))
            throw std::runtime_error(
                    "btl::RunLoop: a default run loop already exists");

        isDefault_ = true;
    }

    RunLoop::~RunLoop()
    {
        // Clear the default registration, but only if it still points at us --
        // another loop must never have its registration cleared here.
        if (isDefault_)
        {
            RunLoop* expected = this;
            g_defaultRunLoop.compare_exchange_strong(expected, nullptr);
        }

        if (impl_)
            impl_->stop();
    }

    RunLoop RunLoop::makeDefault()
    {
        return RunLoop(DefaultTag{});
    }

    RunLoop& RunLoop::getDefault()
    {
        RunLoop* loop = g_defaultRunLoop.load();
        if (!loop)
            throw std::runtime_error(
                    "btl::RunLoop: no default run loop registered");

        return *loop;
    }

    RunLoop* RunLoop::tryGetDefault() noexcept
    {
        return g_defaultRunLoop.load();
    }

    void RunLoop::run()
    {
        auto impl = impl_; // Keep the loop alive if the owner is dropped mid-run.
        impl->run();
    }

    void RunLoop::post(std::function<void(Controller&)> task)
    {
        impl_->post(std::move(task));
    }

    void RunLoop::stop()
    {
        impl_->stop();
    }

    RunLoop::Source RunLoop::Controller::addReadable(NativeHandle handle,
            std::function<void(Controller&)> onReadable)
    {
        SourceId id = impl_->addReadable(handle, std::move(onReadable));
        return Source(impl_->weak_from_this(), id);
    }

    RunLoop::Source RunLoop::Controller::addWritable(NativeHandle handle,
            std::function<void(Controller&)> onWritable)
    {
        SourceId id = impl_->addWritable(handle, std::move(onWritable));
        return Source(impl_->weak_from_this(), id);
    }

    RunLoop::Timer RunLoop::Controller::addTimer(std::chrono::microseconds delay,
            std::function<void(Controller&)> callback)
    {
        TimerId id = impl_->addTimer(delay, std::move(callback));
        return Timer(impl_->weak_from_this(), id);
    }

    void RunLoop::Controller::post(std::function<void(Controller&)> task)
    {
        impl_->post(std::move(task));
    }

    void RunLoop::Controller::stop()
    {
        impl_->stop();
    }

    void RunLoop::Controller::remove(SourceId id)
    {
        impl_->remove(id);
    }

    void RunLoop::Controller::cancel(TimerId id)
    {
        impl_->cancel(id);
    }

    RunLoop::Source::Source(Source&& other) noexcept :
        weak_(std::move(other.weak_)), id_(other.id_)
    {
        other.id_ = 0;
    }

    RunLoop::Source& RunLoop::Source::operator=(Source&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            weak_ = std::move(other.weak_);
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }

    RunLoop::Source::~Source()
    {
        reset();
    }

    void RunLoop::Source::reset() noexcept
    {
        if (id_ == 0)
            return;

        if (auto impl = weak_.lock())
            impl->requestRemove(id_);

        weak_.reset();
        id_ = 0;
    }

    RunLoop::Timer::Timer(Timer&& other) noexcept :
        weak_(std::move(other.weak_)), id_(other.id_)
    {
        other.id_ = 0;
    }

    RunLoop::Timer& RunLoop::Timer::operator=(Timer&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            weak_ = std::move(other.weak_);
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }

    RunLoop::Timer::~Timer()
    {
        reset();
    }

    void RunLoop::Timer::reset() noexcept
    {
        if (id_ == 0)
            return;

        if (auto impl = weak_.lock())
            impl->requestCancel(id_);

        weak_.reset();
        id_ = 0;
    }
}
