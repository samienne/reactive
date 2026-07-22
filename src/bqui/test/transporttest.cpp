#include <bqui/agent/transport.h>

#include "agent/tcptransport.h"

#include <gtest/gtest.h>

#include <memory>
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
