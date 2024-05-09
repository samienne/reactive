#include <reactive/stream/collect.h>
#include <reactive/stream/iterate.h>
#include <reactive/stream/pipe.h>
#include <reactive/stream/sharedstream.h>
#include <reactive/stream/stream.h>

#include <reactive/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <utility>
#include <string>
#include <chrono>

using namespace reactive;
using namespace reactive::stream;

using us = std::chrono::microseconds;
using ms = std::chrono::milliseconds;

TEST(Stream, stream)
{
    auto p = pipe<int>();

    bool called = false;
    auto s = std::move(p.stream);
    auto s2 = std::move(s).fmap([&called](int)
        {
            called = true;
            return std::string("test");
        });

    static_assert(std::is_same<decltype(s2), Stream<std::string>>::value, "");

    p.handle.push(10);
    EXPECT_TRUE(called);
}

TEST(Stream, reference)
{
    auto p = pipe<std::string const&>();

    bool called = false;
    auto s = std::move(p.stream)
        .fmap([&called](std::string const& s)
            {
                called = true;
                return s.size();
            });

    static_assert(std::is_same<decltype(s), Stream<size_t>>::value, "");

    p.handle.push("test");
    EXPECT_TRUE(called);
}

TEST(Stream, uniquePtr)
{
    auto p = pipe<std::unique_ptr<std::string>>();

    auto s = std::move(p.stream)
        .fmap([](std::unique_ptr<std::string> ptr)
            {
                return ptr;
            });

    p.handle.push(std::make_unique<std::string>("test"));
}

TEST(Stream, share)
{
    auto p = pipe<std::string>();

    auto s = std::move(p.stream).share();

    std::vector<SharedStream<std::string>> streams;
    for (int i = 0; i < 10; ++i)
        streams.push_back(s);

    std::vector<Stream<std::string>> streams2;
    int count = 0;
    for (auto&& stream : streams)
    {
        streams2.push_back(std::move(stream).fmap([&count](std::string s)
            {
                ++count;
                return s;
            }));
    }

    p.handle.push("test");

    EXPECT_EQ(10, count);
}

TEST(Stream, convertShared)
{
    auto p = pipe<std::string>();

    Stream<std::string> s = std::move(p.stream).share();
}

TEST(Stream, collect)
{
    auto p = pipe<std::string>();

    auto s = collect(std::move(p.stream));

    static_assert(signal::checkSignal<decltype(s.unwrap())>());

    auto c = signal::makeSignalContext(s);

    EXPECT_EQ(std::vector<std::string>(), c.evaluate());

    p.handle.push("test1");

    auto r = c.update(signal::FrameInfo(1, {}));
    EXPECT_TRUE(r.didChange);

    auto r1 = c.evaluate();
    EXPECT_EQ(1, r1.size());
    EXPECT_EQ("test1", r1.at(0));
}

TEST(Stream, iterate)
{
    auto values = pipe<std::string>();
    auto initial = signal::makeInput<std::string>("hello");

    auto s = iterate([](std::string current, std::string event)
            {
                return current + event;
            },
            initial.signal,
            values.stream
            );

    static_assert(signal::checkSignal<decltype(s.unwrap())>());

    auto c = signal::makeSignalContext(s);
    auto v = c.evaluate();

    EXPECT_EQ("hello", v);

    auto r = c.update(signal::FrameInfo(1, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("hello", v);

    values.handle.push("world");

    r = c.update(signal::FrameInfo(2, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("helloworld", v);

    initial.handle.set("");
    r = c.update(signal::FrameInfo(3, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("", v);

    initial.handle.set("");
    values.handle.push("bye");
    values.handle.push("world");

    r = c.update(signal::FrameInfo(3, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal::FrameInfo(4, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal::FrameInfo(5, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal::FrameInfo(6, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);
}

