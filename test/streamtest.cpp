#include <reactive/stream/hold.h>
#include <reactive/stream/collect.h>
#include <reactive/stream/iterate.h>
#include <reactive/stream/pipe.h>
#include <reactive/stream/sharedstream.h>
#include <reactive/stream/stream.h>

#include <reactive/signal2/signalcontext.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>

using namespace reactive;
using namespace reactive::stream;

using us = std::chrono::microseconds;
using ms = std::chrono::milliseconds;

/*
static_assert(signal::CheckSignal<Hold<int>>::value, "");

static_assert(std::is_same<std::vector<int>,
        signal::signal_value_t<
            Collect<int>
            >
        >::value, "");
*/

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

/*
TEST(Stream, hold)
{
    auto p = pipe<std::string>();

    auto s = hold(std::move(p.stream), std::string("test1"));

    EXPECT_EQ("test1", s.evaluate());

    p.handle.push("test2");

    EXPECT_EQ("test1", s.evaluate());

    signal::FrameInfo info(1, us(1000000));
    signal::update(s, info);

    EXPECT_EQ("test2", s.evaluate());
}
*/

/*
TEST(Stream, collect)
{
    auto p = pipe<std::string>();

    auto s = collect(std::move(p.stream));

    EXPECT_EQ(std::vector<std::string>(), s.evaluate());

    p.handle.push("test1");

    signal::update(s, signal::FrameInfo(1, us(1000000)));

    auto r1 = s.evaluate();
    EXPECT_EQ(1, r1.size());
    EXPECT_EQ("test1", r1.at(0));
}
*/

TEST(Stream, collect2)
{
    auto p = pipe<std::string>();

    auto s = collect2(std::move(p.stream));

    auto c = signal2::makeSignalContext(s);

    EXPECT_EQ(std::vector<std::string>(), c.evaluate());

    p.handle.push("test1");

    auto r = c.update(signal2::FrameInfo(1, {}));
    EXPECT_TRUE(r.didChange);

    auto r1 = c.evaluate();
    EXPECT_EQ(1, r1.size());
    EXPECT_EQ("test1", r1.at(0));
}

TEST(Stream, iterate)
{
    auto values = pipe<std::string>();
    auto initial = signal2::makeInput<std::string>("hello");

    auto s = iterate([](std::string current, std::string event)
            {
                return current + event;
            },
            initial.signal,
            values.stream
            );

    auto c = signal2::makeSignalContext(s);
    auto v = c.evaluate();

    EXPECT_EQ("hello", v);

    auto r = c.update(signal2::FrameInfo(1, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("hello", v);

    values.handle.push("world");

    r = c.update(signal2::FrameInfo(2, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("helloworld", v);

    initial.handle.set("");
    r = c.update(signal2::FrameInfo(3, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("", v);

    initial.handle.set("");
    values.handle.push("bye");
    values.handle.push("world");

    r = c.update(signal2::FrameInfo(3, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal2::FrameInfo(4, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal2::FrameInfo(5, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);

    r = c.update(signal2::FrameInfo(6, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byeworld", v);
}

#if 0
TEST(Stream, stream)
{
    auto p = stream::pipe<int>();

    EXPECT_FALSE(p.output.hasChanged());
    p.output.beginTransaction();
    p.input.push(10);
    p.input.push(20);
    p.input.push(30);
    p.output.endTransaction();

    EXPECT_TRUE(p.output.hasChanged());

    auto r = p.output.getRange();
    auto i = r.begin();
    EXPECT_NE(i, r.end());
    EXPECT_EQ(10, *i++);
    EXPECT_NE(i, r.end());
    EXPECT_EQ(20, *i++);
    EXPECT_NE(i, r.end());
    EXPECT_EQ(30, *i++);
    EXPECT_TRUE(p.output.hasChanged());

    p.output.beginTransaction();
    p.output.endTransaction();
    EXPECT_FALSE(p.output.hasChanged());

    p.input.push(10);
    EXPECT_FALSE(p.output.hasChanged());
}

TEST(Stream, streamRange)
{
    auto pipe = stream::pipe<int>();

    EXPECT_FALSE(pipe.output.hasChanged());
    pipe.output.beginTransaction();
    for (auto i = 0; i < 20000; ++i)
    {
        pipe.input.push(i);
    }
    pipe.output.endTransaction();

    int n = 0;
    for (auto const& i : pipe.output.getRange())
    {
        EXPECT_EQ(n, i);
        ++n;
    }

    EXPECT_EQ(20000, n);
}

TEST(Stream, streamThreaded)
{
    auto pipe = stream::pipe<int>();
    pipe.output.beginTransaction();
    pipe.output.endTransaction();

    auto f = [&pipe]()
    {
        for (auto i = 0; i < 1000; ++i)
        {
            pipe.input.push(i);
        }
    };

    std::thread t1(f);
    std::thread t2(f);

    t1.join();
    t2.join();

    std::thread t3(f);

    pipe.output.beginTransaction();
    pipe.output.endTransaction();

    int n = 0;
    int sum = 0;
    for (auto i : pipe.output.getRange())
    {
        sum += i;
        ++n;
    }

    t3.join();

    EXPECT_TRUE(n >= 2000 && n <= 3000);
}

TEST(Stream, holdSignal)
{
    auto pipe = stream::pipe<int>();

    auto sig = hold(pipe.output, 0);

    EXPECT_FALSE(sig.hasChanged());

    pipe.input.push(10);
    EXPECT_FALSE(sig.hasChanged());
    signal::update(sig, signal::FrameInfo(1, us(0)));

    EXPECT_TRUE(sig.hasChanged());
    EXPECT_EQ(10, sig.evaluate());

    EXPECT_TRUE(sig.hasChanged());

    EXPECT_EQ(10, sig.evaluate());

    pipe.input.push(20);
    pipe.input.push(30);
    pipe.input.push(40);
    signal::update(sig, signal::FrameInfo(2, us(0)));

    EXPECT_TRUE(sig.hasChanged());
    EXPECT_EQ(40, sig.evaluate());
}

TEST(Stream, typeReduction)
{
    auto pipe = stream::pipe<int>();

    auto stream = pipe.output;

    auto s1 = hold(stream, 10);

    EXPECT_FALSE(s1.hasChanged());
    EXPECT_EQ(10, s1.evaluate());

    pipe.input.push(20);
    signal::update(s1, signal::FrameInfo(1, us(0)));

    EXPECT_TRUE(s1.hasChanged());
    EXPECT_EQ(20, s1.evaluate());
}

TEST(Stream, deletionTest)
{
    static size_t count = 0;

    struct Test
    {
        Test()
        {
            ++count;
        }

        Test(Test const&)
        {
            ++count;
        }

        Test& operator=(Test const&)
        {
            return *this;
        }

        ~Test()
        {
            --count;
        }
    };

    auto pipe = stream::pipe<Test>();

    auto stream = pipe.output;

    pipe.output.beginTransaction();
    pipe.input.push(Test());
    pipe.input.push(Test());
    pipe.output.endTransaction();

    EXPECT_EQ(2u, count);

    pipe.output.beginTransaction();
    pipe.input.push(Test());
    pipe.input.push(Test());
    pipe.input.push(Test());
    pipe.input.push(Test());
    pipe.output.endTransaction();
    EXPECT_EQ(4u, count);

    pipe.output.beginTransaction();
    pipe.output.endTransaction();

    EXPECT_EQ(0u, count);
}

TEST(Stream, sharing)
{
    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto pipe = stream::pipe<int>();

    auto stream = pipe.output;

    auto s1 = iterate(add, 10, stream);
    auto s2 = iterate(add, 10, stream);

    EXPECT_EQ(s1.evaluate(), 10);
    EXPECT_EQ(s2.evaluate(), 10);

    pipe.input.push(20);
    signal::update(s1, signal::FrameInfo(1, us(0)));

    EXPECT_EQ(30, s1.evaluate());

    EXPECT_EQ(10, s2.evaluate());

    pipe.input.push(30);
    signal::update(s1, signal::FrameInfo(2, us(0)));
    EXPECT_EQ(60, s2.evaluate());
}

TEST(Stream, map)
{
    auto p = stream::pipe<int>();

    auto t = [](int i)
    {
        return "test " + std::to_string(i);
    };

    auto m = stream::map(t, p.output);

    m.beginTransaction();
    for (auto i = 0; i < 10; ++i)
        p.input.push(i);
    m.endTransaction();

    int i = 0;
    for (auto&& s : m.getRange())
    {
        EXPECT_EQ((std::string("test ") + std::to_string(i)), s);
        ++i;
    }
}

TEST(Stream, filter)
{
    auto p = stream::pipe<int>();

    auto t = [](int i) -> bool
    {
        return i % 2;
    };

    auto m = filter(t, p.output);

    m.beginTransaction();
    for (auto i = 0; i < 10; ++i)
        p.input.push(i);
    m.endTransaction();

    int i = 1;
    for (auto&& s : m.getRange())
    {
        EXPECT_EQ(i, s);
        i += 2;
    }
}
#endif

