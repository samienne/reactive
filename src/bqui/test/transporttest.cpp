#include <bqui/agent/transport.h>

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
