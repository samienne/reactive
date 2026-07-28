#include "runloopimpl.h"

#include <btl/posix/nativehandle_posix.h>

#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace btl
{
namespace
{
    using Clock = std::chrono::steady_clock;

    void setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    class PosixRunLoop final : public RunLoopImpl
    {
    public:
        PosixRunLoop()
        {
            if (pipe(wakeup_) == 0)
            {
                setNonBlocking(wakeup_[0]);
                setNonBlocking(wakeup_[1]);
            }
        }

        ~PosixRunLoop() override
        {
            if (wakeup_[0] >= 0)
                ::close(wakeup_[0]);
            if (wakeup_[1] >= 0)
                ::close(wakeup_[1]);
        }

        SourceId addReadable(NativeHandle handle,
                Callback onReadable) override
        {
            return addFd(handle, std::move(onReadable), /*write=*/false);
        }

        SourceId addWritable(NativeHandle handle,
                Callback onWritable) override
        {
            return addFd(handle, std::move(onWritable), /*write=*/true);
        }

        TimerId addTimer(std::chrono::microseconds delay,
                Callback callback) override
        {
            TimerId id = nextId_++;
            timers_[id] = Timer{ Clock::now() + delay, std::move(callback) };
            return id;
        }

        void post(Callback task) override
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                posted_.push_back(std::move(task));
            }
            wake();
        }

        void remove(SourceId id) override
        {
            sources_.erase(id);
        }

        void cancel(TimerId id) override
        {
            timers_.erase(id);
        }

        void run() override
        {
            if (running_.exchange(true))
                throw std::logic_error("btl::RunLoop::run() is already running");
            enterLoopThread();
            while (running_)
            {
                drainPipe();
                drainPosts();
                if (!running_)
                    break;

                fd_set readSet;
                fd_set writeSet;
                FD_ZERO(&readSet);
                FD_ZERO(&writeSet);

                int maxFd = wakeup_[0];
                FD_SET(wakeup_[0], &readSet);

                for (auto& entry : sources_)
                {
                    int fd = entry.second.fd;
                    FD_SET(fd, entry.second.write ? &writeSet : &readSet);
                    maxFd = std::max(maxFd, fd);
                }

                timeval tv;
                timeval* timeout = nextTimeout(tv);

                int ready = ::select(maxFd + 1, &readSet, &writeSet, nullptr,
                        timeout);

                if (ready > 0)
                    fireReady(readSet, writeSet);

                fireExpiredTimers();
            }
        }

        void stop() override
        {
            running_ = false;
            wake();
        }

    private:
        struct Source
        {
            int fd;
            bool write;
            Callback callback;
        };

        struct Timer
        {
            Clock::time_point deadline;
            Callback callback;
        };

        SourceId addFd(NativeHandle handle, Callback callback,
                bool write)
        {
            SourceId id = nextId_++;
            sources_[id] = Source{ toFd(handle), write, std::move(callback) };
            return id;
        }

        void wake()
        {
            char byte = 0;
            ssize_t n = ::write(wakeup_[1], &byte, 1);
            (void)n;
        }

        void drainPipe()
        {
            char buffer[64];
            while (::read(wakeup_[0], buffer, sizeof(buffer)) > 0)
            {
            }
        }

        void drainPosts()
        {
            std::vector<Callback> tasks;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                tasks.swap(posted_);
            }
            for (auto& task : tasks)
            {
                invoke(task);
                if (!running_)
                    return;
            }
        }

        void fireReady(fd_set& readSet, fd_set& writeSet)
        {
            // Snapshot the ids: a callback may add or remove sources.
            std::vector<SourceId> ready;
            for (auto& entry : sources_)
            {
                int fd = entry.second.fd;
                fd_set& set = entry.second.write ? writeSet : readSet;
                if (FD_ISSET(fd, &set))
                    ready.push_back(entry.first);
            }

            for (SourceId id : ready)
            {
                auto it = sources_.find(id);
                if (it == sources_.end())
                    continue; // removed by an earlier callback this pass.

                Callback callback = it->second.callback;
                invoke(callback);
                if (!running_)
                    return;
            }
        }

        timeval* nextTimeout(timeval& tv)
        {
            if (timers_.empty())
                return nullptr; // block until a source fires.

            auto soonest = timers_.begin()->second.deadline;
            for (auto& entry : timers_)
                soonest = std::min(soonest, entry.second.deadline);

            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                    soonest - Clock::now()).count();
            if (us < 0)
                us = 0;

            tv.tv_sec = (long)(us / 1000000);
            tv.tv_usec = (long)(us % 1000000);
            return &tv;
        }

        void fireExpiredTimers()
        {
            auto now = Clock::now();

            std::vector<TimerId> due;
            for (auto& entry : timers_)
                if (entry.second.deadline <= now)
                    due.push_back(entry.first);

            for (TimerId id : due)
            {
                auto it = timers_.find(id);
                if (it == timers_.end())
                    continue;

                Callback callback = std::move(it->second.callback);
                timers_.erase(it); // one-shot.
                invoke(callback);
                if (!running_)
                    return;
            }
        }

        int wakeup_[2] = { -1, -1 };
        std::map<SourceId, Source> sources_;
        std::map<TimerId, Timer> timers_;
        std::mutex mutex_;
        std::vector<Callback> posted_;
        std::uint64_t nextId_ = 1;
        std::atomic<bool> running_{ false };
    };
}

    std::shared_ptr<RunLoopImpl> makePlatformSpecificRunLoopImpl()
    {
        return std::make_shared<PosixRunLoop>();
    }
}
