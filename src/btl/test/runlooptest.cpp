#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <btl/win32/nativehandle_win32.h>
#include <ws2tcpip.h>
using Sock = SOCKET;
#else
#include <btl/posix/nativehandle_posix.h>
#include <sys/socket.h>
#include <unistd.h>
using Sock = int;
#endif

using namespace std::chrono_literals;
using Controller = btl::RunLoop::Controller;

namespace
{
    btl::NativeHandle wrap(Sock s)
    {
#ifdef _WIN32
        return btl::fromSocket(s);
#else
        return btl::fromFd(s);
#endif
    }

    void writeByte(Sock s)
    {
        char byte = 'x';
#ifdef _WIN32
        ::send(s, &byte, 1, 0);
#else
        ssize_t n = ::write(s, &byte, 1);
        (void)n;
#endif
    }

    void closeSock(Sock s)
    {
#ifdef _WIN32
        ::closesocket(s);
#else
        ::close(s);
#endif
    }

    // A connected pair of sockets. On POSIX a socketpair; on Windows a loopback
    // TCP connect (which has no socketpair()).
    std::pair<Sock, Sock> makePair()
    {
#ifdef _WIN32
        SOCKET listener = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        ::bind(listener, (sockaddr*)&addr, sizeof(addr));
        ::listen(listener, 1);

        int len = sizeof(addr);
        ::getsockname(listener, (sockaddr*)&addr, &len);

        SOCKET client = ::socket(AF_INET, SOCK_STREAM, 0);
        ::connect(client, (sockaddr*)&addr, sizeof(addr));
        SOCKET server = ::accept(listener, nullptr, nullptr);
        ::closesocket(listener);
        return { client, server };
#else
        int fds[2] = { -1, -1 };
        ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
        return { fds[0], fds[1] };
#endif
    }
}

TEST(RunLoop, noDefaultBeforeOneIsRegistered)
{
    // A plain RunLoop must not touch the global, so with none made default there
    // is no default: tryGetDefault() is null and getDefault() throws.
    btl::RunLoop plain;
    (void)plain;

    EXPECT_EQ(nullptr, btl::RunLoop::tryGetDefault());
    EXPECT_THROW(btl::RunLoop::getDefault(), std::runtime_error);
}

TEST(RunLoop, makeDefaultRegistersAndClearsOnScopeExit)
{
    ASSERT_EQ(nullptr, btl::RunLoop::tryGetDefault());

    {
        auto loop = btl::RunLoop::makeDefault();
        EXPECT_EQ(&loop, btl::RunLoop::tryGetDefault());
        EXPECT_EQ(&loop, &btl::RunLoop::getDefault());
    }

    // RAII: the registration is released when the loop goes out of scope.
    EXPECT_EQ(nullptr, btl::RunLoop::tryGetDefault());
}

TEST(RunLoop, secondMakeDefaultThrowsWhileFirstLives)
{
    auto loop = btl::RunLoop::makeDefault();

    // A default already exists, so a second makeDefault() fails loudly...
    EXPECT_THROW(btl::RunLoop::makeDefault(), std::runtime_error);

    // ...and the throw left the original default in place.
    EXPECT_EQ(&loop, btl::RunLoop::tryGetDefault());
}

TEST(RunLoop, defaultCanBeRemadeAfterTheFirstIsGone)
{
    {
        auto first = btl::RunLoop::makeDefault();
        EXPECT_EQ(&first, btl::RunLoop::tryGetDefault());
    }

    // The first cleared itself, so a fresh default registers without throwing.
    auto second = btl::RunLoop::makeDefault();
    EXPECT_EQ(&second, btl::RunLoop::tryGetDefault());
}

TEST(RunLoop, timerFiresThenStops)
{
    btl::RunLoop loop;

    bool fired = false;
    loop.post([&](Controller& c)
        {
            c.addTimer(5ms, [&](Controller& cc)
                {
                    fired = true;
                    cc.stop();
                }).detach();
        });

    loop.run(); // returns once stop() runs from the timer.

    EXPECT_TRUE(fired);
}

TEST(RunLoop, postRunsOnTheLoopThread)
{
    btl::RunLoop loop;

    std::atomic<bool> ran{ false };
    std::thread::id taskThread;
    std::thread::id runThread = std::this_thread::get_id();

    std::thread poster([&]
        {
            std::this_thread::sleep_for(10ms);
            loop.post([&](Controller& c)
                {
                    taskThread = std::this_thread::get_id();
                    ran = true;
                    c.stop();
                });
        });

    loop.run(); // the posted task wakes it and runs here.
    poster.join();

    EXPECT_TRUE(ran.load());
    EXPECT_EQ(taskThread, runThread);
}

TEST(RunLoop, readableFiresWhenPeerWrites)
{
    btl::RunLoop loop; // constructing it starts winsock, which makePair needs.

    auto pair = makePair();

    bool readable = false;
    loop.post([&](Controller& c)
        {
            c.addReadable(wrap(pair.first), [&](Controller& cc)
                {
                    readable = true;
                    cc.stop();
                }).detach();
        });

    // Peer writes, so pair.first is readable when the source is registered.
    writeByte(pair.second);

    loop.run();

    EXPECT_TRUE(readable);

    closeSock(pair.first);
    closeSock(pair.second);
}

TEST(RunLoop, droppingSourceHandleRemovesIt)
{
    btl::RunLoop loop;

    auto pair = makePair();

    int fires = 0;
    loop.post([&](Controller& c)
        {
            {
                auto source = c.addReadable(wrap(pair.first), [&](Controller&)
                    {
                        ++fires;
                    });
                // source drops here, on the loop thread, so it is removed before
                // the loop waits - the pending write must not fire it.
            }
            c.addTimer(20ms, [](Controller& t) { t.stop(); }).detach();
        });

    writeByte(pair.second);

    loop.run();

    EXPECT_EQ(fires, 0);

    closeSock(pair.first);
    closeSock(pair.second);
}

#ifdef _WIN32
// A waitable HANDLE (here a manual-reset event, as a named pipe's overlapped
// event would be) is a first-class readable source on Windows.
TEST(RunLoop, readableFiresOnSignaledHandleThenStopsAfterRemove)
{
    btl::RunLoop loop;

    HANDLE event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr); // manual-reset

    int fires = 0;
    btl::RunLoop::Source source;
    loop.post([&](Controller& c)
        {
            source = c.addReadable(btl::fromHandle(event), [&](Controller& cc)
                {
                    ++fires;
                    cc.remove(source.id()); // stop watching; event stays signaled.
                    // If remove() failed, the still-signaled (level-triggered)
                    // event would fire again before this timer stops the loop.
                    cc.addTimer(20ms, [](Controller& t) { t.stop(); }).detach();
                });
        });

    ::SetEvent(event); // signaled before run(): readable from the start.

    loop.run();

    EXPECT_EQ(fires, 1); // fired once, and remove() kept it from re-firing.

    ::CloseHandle(event);
}

// The thread message queue is a readable source: a message posted to the loop
// thread wakes the loop (MsgWaitForMultipleObjectsEx) and fires the source,
// whose callback drains the queue with PeekMessage.
TEST(RunLoop, messageQueueFiresOnPostedThreadMessage)
{
    btl::RunLoop loop;

    std::atomic<DWORD> loopThreadId{ 0 };
    std::atomic<bool> ready{ false };
    bool fired = false; // written on the loop thread, read after join.

    std::thread worker([&]
        {
            loopThreadId = ::GetCurrentThreadId();

            // Force the thread message queue to exist so PostThreadMessage can
            // target it, then let the main thread post.
            MSG msg;
            ::PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
            ready = true;

            loop.post([&](Controller& c)
                {
                    c.addReadable(btl::fromMessageQueue(), [&](Controller& cc)
                        {
                            MSG m;
                            while (::PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE))
                                if (m.message == WM_USER)
                                    fired = true;
                            cc.stop();
                        }).detach();
                });

            loop.run();
        });

    while (!ready.load())
        std::this_thread::sleep_for(1ms);

    // Retry until the queue is up (PostThreadMessage can briefly fail with
    // ERROR_INVALID_THREAD_ID before the target thread has a queue).
    while (!::PostThreadMessageW(loopThreadId.load(), WM_USER, 0, 0))
        std::this_thread::sleep_for(1ms);

    worker.join();

    EXPECT_TRUE(fired);
}

namespace
{
    bool inMoveSizeLoop()
    {
        GUITHREADINFO info{};
        info.cbSize = sizeof(info);
        return ::GetGUIThreadInfo(::GetCurrentThreadId(), &info)
            && (info.flags & GUI_INMOVESIZE);
    }

    /**
     * Never shown: the move loop runs on it without anything on screen.
     */
    HWND makeHiddenWindow()
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ::DefWindowProcW;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.lpszClassName = L"btlRunLoopTestWindow";
        ::RegisterClassW(&wc); // fails harmlessly once already registered.
        return ::CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                0, 0, 200, 200, nullptr, nullptr, wc.hInstance, nullptr);
    }

    void endMoveLoop(HWND hwnd)
    {
        ::PostMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
    }

    /**
     * Registers the message source and, on its first fire, runs `body` under
     * it. A watchdog ends a modal loop that `body` failed to end.
     */
    struct MessageSourceHarness
    {
        template <typename Body>
        void run(btl::RunLoop& loop, Body body)
        {
            hwnd = makeHiddenWindow();
            ASSERT_NE(hwnd, nullptr);

            loop.post([&](Controller& c)
                {
                    c.addReadable(btl::fromMessageQueue(),
                        [&](Controller& cc)
                        {
                            MSG m;
                            bool start = false;
                            while (::PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE))
                            {
                                if (m.message == WM_USER)
                                    start = true;
                                else
                                    ::DispatchMessageW(&m);
                            }
                            if (start)
                                body(cc);
                        }).detach();
                    ::PostMessageW(hwnd, WM_USER, 0, 0);
                });

            std::atomic<bool> done{ false };
            std::thread watchdog([&]
                {
                    auto deadline = std::chrono::steady_clock::now() + 5s;
                    while (!done && std::chrono::steady_clock::now() < deadline)
                        std::this_thread::sleep_for(5ms);
                    if (done)
                        return;
                    watchdogFired = true;
                    endMoveLoop(hwnd);
                    loop.stop();
                });

            auto finish = [&]
                {
                    done = true;
                    watchdog.join();
                    ::DestroyWindow(hwnd);
                };
            try
            {
                loop.run();
            }
            catch (...)
            {
                finish();
                throw;
            }
            finish();
        }

        HWND hwnd = nullptr;
        std::atomic<bool> watchdogFired{ false };
    };
}

// DefWindowProc's move loop (here the keyboard one, via SC_MOVE) blocks the
// message-source callback; expired timers and posts from another thread still
// run inside it.
TEST(RunLoop, moveLoopUnderMessageSourceRunsTimersAndPosts)
{
    btl::RunLoop loop;
    MessageSourceHarness harness;

    std::atomic<bool> timerRan{ false };
    std::atomic<bool> finished{ false };
    bool timerInMoveLoop = false;
    bool postInMoveLoop = false;
    bool postRan = false;
    std::thread poster;

    harness.run(loop, [&](Controller& c)
        {
            c.addTimer(20ms, [&](Controller&)
                {
                    timerInMoveLoop = inMoveSizeLoop();
                    timerRan = true;
                }).detach();

            poster = std::thread([&]
                {
                    while (!timerRan && !finished)
                        std::this_thread::sleep_for(1ms);
                    if (!timerRan)
                        return;
                    loop.post([&](Controller&)
                        {
                            postRan = true;
                            postInMoveLoop = inMoveSizeLoop();
                            endMoveLoop(harness.hwnd);
                        });
                });

            ::SendMessageW(harness.hwnd, WM_SYSCOMMAND, SC_MOVE, 0);
            c.stop();
        });
    finished = true;
    if (poster.joinable())
        poster.join();

    EXPECT_FALSE(harness.watchdogFired.load());
    EXPECT_TRUE(timerRan.load());
    EXPECT_TRUE(timerInMoveLoop);
    EXPECT_TRUE(postRan);
    EXPECT_TRUE(postInMoveLoop);
}

// stop() from a callback run inside the move loop: nothing more runs inside
// it, and run() returns once the move loop ends.
TEST(RunLoop, stopInsideMoveLoopEndsRunAfterIt)
{
    btl::RunLoop loop;
    MessageSourceHarness harness;

    int ranInMoveLoop = 0;
    bool stoppedInMoveLoop = false;
    bool returnedFromMoveLoop = false;

    harness.run(loop, [&](Controller& c)
        {
            c.addTimer(20ms, [&](Controller& cc)
                {
                    stoppedInMoveLoop = inMoveSizeLoop();
                    cc.stop();
                    auto count = [&](Controller&)
                        {
                            if (inMoveSizeLoop())
                                ++ranInMoveLoop;
                        };
                    cc.post(count);
                    cc.addTimer(0ms, count).detach();
                    endMoveLoop(harness.hwnd);
                }).detach();

            ::SendMessageW(harness.hwnd, WM_SYSCOMMAND, SC_MOVE, 0);
            returnedFromMoveLoop = true;
        });

    EXPECT_FALSE(harness.watchdogFired.load());
    EXPECT_TRUE(stoppedInMoveLoop);
    EXPECT_TRUE(returnedFromMoveLoop);
    EXPECT_EQ(ranInMoveLoop, 0);
}

// An exception thrown by a callback run inside the move loop cannot cross the
// timer callback; it surfaces from run() once the move loop ends.
TEST(RunLoop, exceptionInsideMoveLoopRethrowsFromRun)
{
    btl::RunLoop loop;
    MessageSourceHarness harness;

    bool thrownInMoveLoop = false;
    bool returnedFromMoveLoop = false;

    EXPECT_THROW(harness.run(loop, [&](Controller& c)
        {
            c.addTimer(20ms, [&](Controller&)
                {
                    thrownInMoveLoop = inMoveSizeLoop();
                    endMoveLoop(harness.hwnd);
                    throw std::runtime_error("from the move loop");
                }).detach();

            ::SendMessageW(harness.hwnd, WM_SYSCOMMAND, SC_MOVE, 0);
            returnedFromMoveLoop = true;
        }), std::runtime_error);

    EXPECT_FALSE(harness.watchdogFired.load());
    EXPECT_TRUE(thrownInMoveLoop);
    EXPECT_TRUE(returnedFromMoveLoop);
}

// The move loop's exception happened first, so it is the one run() rethrows
// when the message-source callback throws as well.
TEST(RunLoop, exceptionInsideMoveLoopWinsOverMessageSourceException)
{
    btl::RunLoop loop;
    MessageSourceHarness harness;

    EXPECT_THROW(harness.run(loop, [&](Controller& c)
        {
            c.addTimer(20ms, [&](Controller&)
                {
                    endMoveLoop(harness.hwnd);
                    throw std::runtime_error("from the move loop");
                }).detach();

            ::SendMessageW(harness.hwnd, WM_SYSCOMMAND, SC_MOVE, 0);
            throw std::logic_error("from the message source");
        }), std::runtime_error);

    EXPECT_FALSE(harness.watchdogFired.load());
}

// Only the move/size loop runs callbacks: another nested message loop under
// the message source (a dialog opened by an input handler) leaves them queued.
TEST(RunLoop, otherModalLoopUnderMessageSourceRunsNothing)
{
    btl::RunLoop loop;
    MessageSourceHarness harness;

    int ranInside = 0;
    int ranAfter = 0;
    bool inside = false;

    harness.run(loop, [&](Controller& c)
        {
            auto count = [&](Controller&) { ++(inside ? ranInside : ranAfter); };
            c.post(count);
            c.addTimer(0ms, count).detach();
            std::thread([&loop, count] { loop.post(count); }).join();

            inside = true;
            auto end = std::chrono::steady_clock::now() + 100ms;
            while (std::chrono::steady_clock::now() < end)
            {
                ::MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
                MSG m;
                while (::PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE))
                    ::DispatchMessageW(&m);
            }
            inside = false;

            c.addTimer(1ms, [](Controller& cc) { cc.stop(); }).detach();
        });

    EXPECT_FALSE(harness.watchdogFired.load());
    EXPECT_EQ(ranInside, 0);
    EXPECT_EQ(ranAfter, 3);
}
#endif
