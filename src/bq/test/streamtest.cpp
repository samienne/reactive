#include <bq/stream/collect.h>
#include <bq/stream/iterate.h>
#include <bq/stream/pipe.h>
#include <bq/stream/sharedstream.h>
#include <bq/stream/stream.h>

#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <utility>
#include <string>
#include <chrono>

using namespace bq;
using namespace bq::stream;

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

// stream.h:55: share() assigns control_->callback outright (no chaining). So
// share()ing the SAME underlying stream twice makes the second share win and
// silently starves the first. This pins that behaviour down: sharing a stream
// more than once (e.g. collecting the same raw stream in two places) is a bug.
TEST(Stream, shareSameStreamTwice)
{
    auto p = pipe<std::string>();

    Stream<std::string> copy1 = p.stream;
    Stream<std::string> copy2 = p.stream;

    auto shared1 = std::move(copy1).share();
    auto shared2 = std::move(copy2).share();

    int count1 = 0;
    int count2 = 0;
    auto d1 = shared1.fmap([&count1](std::string s) { ++count1; return s; });
    auto d2 = shared2.fmap([&count2](std::string s) { ++count2; return s; });

    p.handle.push("event");

    // The second share() overwrote the first: copy1's shared view is starved.
    EXPECT_EQ(0, count1);
    EXPECT_EQ(1, count2);
}

// Two SignalContexts built from the SAME collect signal, both alive before any
// event: does each independently see every event? (multi-context coexistence)
TEST(Stream, collectInTwoContexts)
{
    auto p = pipe<std::string>();
    auto s = collect(std::move(p.stream));

    auto c1 = signal::makeSignalContext(s);
    auto c2 = signal::makeSignalContext(s);

    p.handle.push("a");

    auto r1 = c1.update(signal::FrameInfo(1, {}));
    auto r2 = c2.update(signal::FrameInfo(1, {}));

    EXPECT_TRUE(r1.didChange);
    EXPECT_TRUE(r2.didChange);
    EXPECT_EQ((std::vector<std::string>{ "a" }), c1.evaluate());
    EXPECT_EQ((std::vector<std::string>{ "a" }), c2.evaluate());
}

// A context created AFTER an event was pushed misses that event permanently
// (streams have no current-value convergence like signal inputs).
TEST(Stream, lateContextMissesPriorEvents)
{
    auto p = pipe<std::string>();
    auto s = collect(std::move(p.stream));

    auto c1 = signal::makeSignalContext(s);

    p.handle.push("early");

    auto c2 = signal::makeSignalContext(s);

    p.handle.push("late");

    c1.update(signal::FrameInfo(1, {}));
    c2.update(signal::FrameInfo(1, {}));

    EXPECT_EQ((std::vector<std::string>{ "early", "late" }), c1.evaluate());
    EXPECT_EQ((std::vector<std::string>{ "late" }), c2.evaluate());
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

    r = c.update(signal::FrameInfo(4, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal::FrameInfo(5, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal::FrameInfo(6, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal::FrameInfo(7, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);
}

