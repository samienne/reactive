#include <bqui/agent/transport.h>

#include "agent/tcptransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#ifdef _WIN32
#   include <process.h>
#else
#   include <unistd.h>
#endif

using namespace bqui::agent;

namespace
{
    int currentPid()
    {
#ifdef _WIN32
        return _getpid();
#else
        return getpid();
#endif
    }

    std::string uniqueEndpoint(std::string const& name)
    {
        auto tag = name + "-" + std::to_string(currentPid());
#ifdef _WIN32
        return "\\\\.\\pipe\\bqui-" + tag;
#else
        return "/tmp/bqui-" + tag + ".sock";
#endif
    }
} // namespace

TEST(transport, loopbackRoundTripsFramesBothDirections)
{
    auto endpoint = uniqueEndpoint("loopback");
    auto listener = listen(endpoint);

    // A frame far larger than any socket/pipe buffer, so it must span several
    // reads and prove the framing reassembles it.
    std::string large(1024 * 1024, 'x');
    for (size_t i = 0; i < large.size(); ++i)
        large[i] = static_cast<char>('a' + (i % 26));

    // The server runs on its own thread and echoes each frame straight back, so
    // a large send never deadlocks against a same-thread receive.
    std::thread serverThread([&]
    {
        auto server = listener->accept();
        while (auto frame = server->receive())
            server->send(*frame);
    });

    auto client = connect(endpoint);

    // Send an empty frame, a small one, then the large one, back-to-back, to
    // prove framing does not desync, and check each echoes back exactly.
    client->send("");
    client->send("hello");
    client->send(large);

    auto r1 = client->receive();
    auto r2 = client->receive();
    auto r3 = client->receive();

    ASSERT_TRUE(r1 && r2 && r3);
    EXPECT_EQ("", *r1);
    EXPECT_EQ("hello", *r2);
    EXPECT_EQ(large, *r3);

    client.reset();
    serverThread.join();
}

TEST(transport, receiveReturnsNulloptOnCleanClose)
{
    auto endpoint = uniqueEndpoint("close");
    auto listener = listen(endpoint);

    std::unique_ptr<Transport> server;
    std::thread acceptThread([&] { server = listener->accept(); });

    auto client = connect(endpoint);
    acceptThread.join();
    ASSERT_TRUE(server);

    client->send("bye");
    client.reset();

    auto received = server->receive();
    ASSERT_TRUE(received);
    EXPECT_EQ("bye", *received);

    // The peer is gone, so the next receive reports a clean close.
    EXPECT_FALSE(server->receive().has_value());
}

TEST(transport, closeUnblocksABlockedReceive)
{
    // The session's shutdown hinge: a reader parked in receive() with nothing
    // arriving must be woken by close() from another thread, so it can be
    // joined. This is the exact primitive the app thread relies on to end a run.
    auto endpoint = uniqueEndpoint("closewake");
    auto listener = listen(endpoint);

    std::unique_ptr<Transport> server;
    std::thread acceptThread([&] { server = listener->accept(); });
    auto client = connect(endpoint);
    acceptThread.join();
    ASSERT_TRUE(server);

    std::atomic<bool> returned{ false };
    std::optional<std::string> result;
    std::thread reader([&]
    {
        result = server->receive();
        returned.store(true);
    });

    // Let the reader reach its blocking read, then close. close() must be race
    // free either way: a read already parked, or one about to park, both end.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server->close();

    reader.join(); // Must not hang.
    EXPECT_TRUE(returned.load());
    EXPECT_FALSE(result.has_value());

    client.reset();
}

TEST(transport, sendSucceedsWhileAnotherThreadBlocksInReceive)
{
    // The core of the flow-control model: one thread parks in receive() while
    // another sends on the same transport. A synchronous handle would serialise
    // the two and deadlock; the transport must be genuinely full-duplex.
    auto endpoint = uniqueEndpoint("duplex");
    auto listener = listen(endpoint);

    std::unique_ptr<Transport> server;
    std::thread acceptThread([&] { server = listener->accept(); });
    auto client = connect(endpoint);
    acceptThread.join();
    ASSERT_TRUE(server);

    std::atomic<bool> readerDone{ false };
    std::optional<std::string> got;
    std::thread reader([&]
    {
        got = server->receive(); // No client frame is coming; this parks.
        readerDone.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send while the reader is parked — must complete, not deadlock.
    server->send("ping");
    auto echoed = client->receive();
    ASSERT_TRUE(echoed);
    EXPECT_EQ("ping", *echoed);

    // Unblock and join the parked reader.
    server->close();
    reader.join();
    EXPECT_TRUE(readerDone.load());
    EXPECT_FALSE(got.has_value());

    client.reset();
}

TEST(transport, acceptWaitsForADelayedClient)
{
    // Force the listener to park in accept() before the client appears, so the
    // asynchronous connect path (not the already-connected fast path) is taken.
    auto endpoint = uniqueEndpoint("delayed");
    auto listener = listen(endpoint);

    std::unique_ptr<Transport> server;
    std::thread acceptThread([&] { server = listener->accept(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto client = connect(endpoint);
    acceptThread.join();
    ASSERT_TRUE(server);

    // The connection is usable after the delayed accept.
    client->send("late");
    auto received = server->receive();
    ASSERT_TRUE(received);
    EXPECT_EQ("late", *received);

    client.reset();
}

// --- TCP transport --------------------------------------------------------

TEST(tcpTransport, endpointShapeSelectsTcp)
{
    // TCP shapes parse; a host defaults to loopback when only a port is given.
    auto a = parseTcpEndpoint("tcp://127.0.0.1:1234");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ("127.0.0.1", a->host);
    EXPECT_EQ(1234, a->port);

    auto b = parseTcpEndpoint("127.0.0.1:5678");
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ("127.0.0.1", b->host);
    EXPECT_EQ(5678, b->port);

    auto c = parseTcpEndpoint(":9000");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ("127.0.0.1", c->host);
    EXPECT_EQ(9000, c->port);

    // Pipe and Unix-socket endpoints are not TCP, so the dispatcher falls back.
    EXPECT_FALSE(parseTcpEndpoint("\\\\.\\pipe\\bqui-x").has_value());
    EXPECT_FALSE(parseTcpEndpoint("/tmp/bqui-x.sock").has_value());
}

TEST(tcpTransport, loopbackRoundTripsFramesBothDirections)
{
    // An ephemeral port: bind :0, read the chosen port back, connect to it.
    auto listener = tcpListen(*parseTcpEndpoint("127.0.0.1:0"));
    auto endpoint = "127.0.0.1:" + std::to_string(listener->port());

    std::string large(1024 * 1024, 'x');
    for (size_t i = 0; i < large.size(); ++i)
        large[i] = static_cast<char>('a' + (i % 26));

    // The echo server runs on its own thread, so a large send from the client
    // never deadlocks against a same-thread receive: a concurrent send and
    // receive across the socket proves the transport is full-duplex.
    std::thread serverThread([&]
    {
        auto server = listener->accept();
        while (auto frame = server->receive())
            server->send(*frame);
    });

    // connect() dispatches on the endpoint shape and picks TCP here.
    auto client = connect(endpoint);

    client->send("");
    client->send("hello");
    client->send(large);

    auto r1 = client->receive();
    auto r2 = client->receive();
    auto r3 = client->receive();

    ASSERT_TRUE(r1 && r2 && r3);
    EXPECT_EQ("", *r1);
    EXPECT_EQ("hello", *r2);
    EXPECT_EQ(large, *r3);

    client.reset();
    serverThread.join();
}

TEST(tcpTransport, receiveReturnsNulloptOnCleanClose)
{
    auto listener = tcpListen(*parseTcpEndpoint("127.0.0.1:0"));
    auto endpoint = "127.0.0.1:" + std::to_string(listener->port());

    std::unique_ptr<Transport> server;
    std::thread acceptThread([&] { server = listener->accept(); });

    auto client = connect(endpoint);
    acceptThread.join();
    ASSERT_TRUE(server);

    client->send("bye");
    client.reset();

    auto received = server->receive();
    ASSERT_TRUE(received);
    EXPECT_EQ("bye", *received);

    // The peer is gone, so the next receive reports a clean close.
    EXPECT_FALSE(server->receive().has_value());
}
