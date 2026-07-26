#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
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

TEST(RunLoop, timerFiresThenStops)
{
    btl::RunLoop loop;

    bool fired = false;
    loop.addTimer(5ms, [&]
        {
            fired = true;
            loop.stop();
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
            loop.post([&]
                {
                    taskThread = std::this_thread::get_id();
                    ran = true;
                    loop.stop();
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
    loop.addReadable(wrap(pair.first), [&]
        {
            readable = true;
            loop.stop();
        });

    // Peer writes, so pair.first is readable when the loop starts.
    writeByte(pair.second);

    loop.run();

    EXPECT_TRUE(readable);

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
    btl::SourceId id = loop.addReadable(btl::fromHandle(event), [&]
        {
            ++fires;
            loop.remove(id); // stop watching; the event stays signaled.
            // If remove() failed, the still-signaled (level-triggered) event
            // would fire again before this timer stops the loop.
            loop.addTimer(20ms, [&] { loop.stop(); });
        });

    ::SetEvent(event); // signaled before run(): readable from the start.

    loop.run();

    EXPECT_EQ(fires, 1); // fired once, and remove() kept it from re-firing.

    ::CloseHandle(event);
}
#endif
