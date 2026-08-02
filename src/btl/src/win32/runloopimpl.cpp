// These must precede any header that pulls in winsock2/windows (below).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "runloopimpl.h"

#include <btl/win32/nativehandle_win32.h>

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace btl
{
namespace
{
    using Clock = std::chrono::steady_clock;

    // WSAStartup is refcounted; one guard per RunLoop keeps winsock up for its
    // lifetime without caring who else started it.
    struct WinsockGuard
    {
        WinsockGuard()
        {
            WSADATA data;
            WSAStartup(MAKEWORD(2, 2), &data);
        }

        ~WinsockGuard()
        {
            WSACleanup();
        }
    };

    class Win32RunLoop final : public RunLoopImpl
    {
    public:
        Win32RunLoop()
        {
            wakeup_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }

        ~Win32RunLoop() override
        {
            for (auto& entry : sources_)
                if (entry.second.ownsEvent)
                    WSACloseEvent(entry.second.event);
            if (wakeup_)
                CloseHandle(wakeup_);
        }

        SourceId addReadable(NativeHandle handle,
                Callback onReadable) override
        {
            if (handle.kind() == NativeHandle::Kind::MessageQueue)
                return addMessage(std::move(onReadable));
            if (handle.kind() == NativeHandle::Kind::Handle)
                return addHandle(handle, std::move(onReadable));
            return addSocket(handle, std::move(onReadable),
                    FD_READ | FD_ACCEPT | FD_CLOSE);
        }

        SourceId addWritable(NativeHandle handle,
                Callback onWritable) override
        {
            if (handle.kind() == NativeHandle::Kind::Handle)
                return addHandle(handle, std::move(onWritable));
            return addSocket(handle, std::move(onWritable), FD_WRITE | FD_CLOSE);
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
            SetEvent(wakeup_);
        }

        void remove(SourceId id) override
        {
            auto it = sources_.find(id);
            if (it != sources_.end())
            {
                if (it->second.ownsEvent)
                    WSACloseEvent(it->second.event);
                sources_.erase(it);
            }
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
                ResetEvent(wakeup_);
                drainPosts();
                if (!running_)
                    break;

                std::vector<HANDLE> handles;
                std::vector<SourceId> ids;
                handles.push_back(wakeup_);

                bool hasMessages = false;
                SourceId messageId = 0;
                for (auto& entry : sources_)
                {
                    if (entry.second.isMessage)
                    {
                        // Waited on via the message queue, not a HANDLE.
                        hasMessages = true;
                        messageId = entry.first;
                        continue;
                    }
                    if (handles.size() >= MAXIMUM_WAIT_OBJECTS)
                        break; // 64-handle WFMO cap; see the design doc.
                    handles.push_back(entry.second.event);
                    ids.push_back(entry.first);
                }

                DWORD timeout = nextTimeout();
                DWORD result;
                if (hasMessages)
                    result = MsgWaitForMultipleObjectsEx(
                            (DWORD)handles.size(), handles.data(), timeout,
                            QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                else
                    result = WaitForMultipleObjects(
                            (DWORD)handles.size(), handles.data(), FALSE,
                            timeout);

                if (result == WAIT_TIMEOUT)
                {
                    fireExpiredTimers();
                }
                else if (hasMessages
                        && result == WAIT_OBJECT_0 + handles.size())
                {
                    // MsgWaitForMultipleObjectsEx reports the message queue as
                    // the object just past the waited handles.
                    fireMessage(messageId);
                    fireExpiredTimers();
                }
                else if (result >= WAIT_OBJECT_0
                        && result < WAIT_OBJECT_0 + handles.size())
                {
                    DWORD index = result - WAIT_OBJECT_0;
                    if (index != 0) // 0 is the wakeup event.
                        fireSocket(ids[index - 1]);
                    fireExpiredTimers();
                }
            }
        }

        void stop() override
        {
            running_ = false;
            SetEvent(wakeup_);
        }

    private:
        struct Source
        {
            HANDLE event;   // a WSAEVENT we own (socket), or a caller HANDLE.
            SOCKET socket;  // INVALID_SOCKET for a handle source.
            bool ownsEvent; // true: we created the event and must close it.
            Callback callback;
            bool isMessage = false; // true: the thread message queue, no event.
        };

        struct Timer
        {
            Clock::time_point deadline;
            Callback callback;
        };

        SourceId addSocket(NativeHandle handle, Callback callback,
                long events)
        {
            SOCKET socket = toSocket(handle);
            WSAEVENT event = WSACreateEvent();
            WSAEventSelect(socket, event, events);

            SourceId id = nextId_++;
            sources_[id] = Source{ event, socket, true, std::move(callback) };
            return id;
        }

        // A handle source waits on a caller-owned waitable HANDLE directly (an
        // overlapped-I/O event, a manual-reset event): no WSAEventSelect, no
        // network-event enumeration, and we never close it.
        SourceId addHandle(NativeHandle handle, Callback callback)
        {
            SourceId id = nextId_++;
            sources_[id] = Source{ toHandle(handle), INVALID_SOCKET, false,
                    std::move(callback) };
            return id;
        }

        // A message source has no waitable event; run() waits on the thread
        // message queue via MsgWaitForMultipleObjectsEx and fires this source
        // when input is available. Its callback drains the queue itself.
        SourceId addMessage(Callback callback)
        {
            SourceId id = nextId_++;
            Source source{ nullptr, INVALID_SOCKET, false,
                    std::move(callback) };
            source.isMessage = true;
            sources_[id] = std::move(source);
            return id;
        }

        void fireSocket(SourceId id)
        {
            auto it = sources_.find(id);
            if (it == sources_.end())
                return;

            // Sockets need their pending network events drained; a handle source
            // has none, its callback drains the I/O (and resets the event).
            if (it->second.socket != INVALID_SOCKET)
            {
                WSANETWORKEVENTS network;
                WSAEnumNetworkEvents(
                        it->second.socket, it->second.event, &network);
            }

            // The callback may remove this or other sources, so copy it out.
            Callback callback = it->second.callback;
            invoke(callback);
        }

        // A message source has no event to drain or reset; the callback pulls
        // messages off the thread queue itself. Level-triggered: anything the
        // callback leaves behind fires the wait again next pass.
        void fireMessage(SourceId id)
        {
            auto it = sources_.find(id);
            if (it == sources_.end())
                return;

            // The callback may remove this or other sources, so copy it out.
            Callback callback = it->second.callback;
            invoke(callback);
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

        DWORD nextTimeout()
        {
            if (timers_.empty())
                return INFINITE;

            auto soonest = timers_.begin()->second.deadline;
            for (auto& entry : timers_)
                soonest = std::min(soonest, entry.second.deadline);

            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    soonest - Clock::now()).count();
            if (ms < 0)
                return 0;
            return (DWORD)ms;
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
                    continue; // cancelled by an earlier callback this pass.

                Callback callback = std::move(it->second.callback);
                timers_.erase(it); // one-shot.
                invoke(callback);
                if (!running_)
                    return;
            }
        }

        WinsockGuard winsock_;
        HANDLE wakeup_ = nullptr;
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
        return std::make_shared<Win32RunLoop>();
    }
}
