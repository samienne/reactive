#include <reactive/signal2/signal.h>
#include <reactive/signal2/constant.h>
#include <reactive/signal2/signalcontext.h>
#include <reactive/signal2/input.h>
#include <reactive/signal2/merge.h>
#include <reactive/signal2/join.h>
#include <reactive/signal2/combine.h>
#include <reactive/signal2/fromoptional.h>

#include <btl/demangle.h>

#include <gtest/gtest.h>

using namespace reactive::signal2;

template <typename T>
class Type
{
public:
    using type = T;

    template <typename... Us>
    auto construct(Us&&... us) const
    {
        return T(std::forward<Us>(us)...);
    }

    template <typename U>
    constexpr bool operator==(Type<U> const&) const
    {
        return std::is_same_v<T, U>;
    }

    template <typename U>
    constexpr bool operator!=(Type<U> const& rhs) const
    {
        return !(*this == rhs);
    }

    std::string toString() const
    {
        return btl::demangle<Type<T>>();
    }

    friend std::ostream& operator<<(std::ostream& s, Type<T> const& t)
    {
        return s << t.toString();
    }
};

template <typename T>
constexpr Type<T> getType(T&&)
{
    return {};
}

template <typename T>
constexpr Type<T&> getType(T&)
{
    return {};
}

template <typename T>
constexpr Type<T const&> getType(T const&)
{
    return {};
}

TEST(Signal2, constant)
{
    auto s = constant(10);
    AnySignal<int> ss = s;

    auto c = makeSignalContext(s);

    EXPECT_EQ(10, c.evaluate());

    EXPECT_EQ(Type<int const&>(), getType(c.evaluate()));

    FrameInfo frame(1, signal_time_t(10));

    auto r = c.update(frame);
    EXPECT_EQ(r.nextUpdate, std::nullopt);
}

TEST(Signal2, signalContext)
{
    auto c = makeSignalContext(constant(42));

    UpdateResult r = c.update(FrameInfo(1, signal_time_t(0)));

    EXPECT_EQ(std::nullopt, r.nextUpdate);

    EXPECT_EQ(Type<int const&>(), getType(c.evaluate()));

    int v = c.evaluate();

    EXPECT_EQ(42, v);
}

TEST(Signal2, signalInput)
{
    auto input = makeInput(42);

    auto c = makeSignalContext(input.signal);

    EXPECT_EQ(Type<SignalContext<int const&>>(), Type<decltype(c)>());

    EXPECT_EQ(42, c.evaluate());

    input.handle.set(22);

    EXPECT_EQ(42, c.evaluate());

    auto r = c.update(FrameInfo(1, {}));
    EXPECT_EQ(std::nullopt, r.nextUpdate);

    EXPECT_EQ(22, c.evaluate());

    EXPECT_EQ(Type<int const&>(), getType(c.evaluate()));
}

TEST(Signal2, map)
{
    auto input = makeInput(42);

    auto s = input.signal.map([](int n)
                {
                    return n * 2;
                });

    auto c = makeSignalContext(std::move(s));

    EXPECT_EQ(84, c.evaluate());

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ(std::nullopt, r.nextUpdate);

    input.handle.set(12);

    EXPECT_EQ(84, c.evaluate());

    r = c.update(FrameInfo(2, {}));

    EXPECT_EQ(24, c.evaluate());
    EXPECT_TRUE(r.didChange);
    EXPECT_EQ(std::nullopt, r.nextUpdate);
}

TEST(Signal2, mapReferenceToTemp)
{
    auto input = makeInput(42);

    auto s1 = input.signal.map([](int n) -> std::unique_ptr<int>
            {
                return std::make_unique<int>(n);
            });

    auto s2 = s1.map([](std::unique_ptr<int> const& n) -> int const&
            {
                return *n;
            });

    auto c = makeSignalContext(std::move(s2));

    EXPECT_EQ(42, c.evaluate());
}

TEST(Signal2, merge)
{
    auto input1 = makeInput(42);
    auto input2 = makeInput<int, std::string>(20, "test");

    auto s = merge(input1.signal, input2.signal);

    Type<Signal<Merge<InputSignal<int>, InputSignal<int, std::string>>,
        int, int, std::string>> type;
    EXPECT_EQ(type, Type<decltype(s)>());

    auto c = makeSignalContext(std::move(s));

    Type<SignalResult<int const&, int const&, std::string const&>> type2;
    EXPECT_EQ(type2, getType(c.evaluate()));

    EXPECT_EQ(42, c.evaluate().get<0>());
    EXPECT_EQ(20, c.evaluate().get<1>());
    EXPECT_EQ("test", c.evaluate().get<2>());
}

TEST(Signal2, share)
{
    auto input = makeInput<std::string>("hello");

    auto s1 = input.signal.map([](std::string const& str)
            {
                return str + " world!";
            });

    auto s2 = s1.share();

    std::vector<SignalContext<std::string>> contexts;
    for (int i = 0; i < 1024; ++i)
    {
        contexts.emplace_back(makeSignalContext(s2));
    }

    std::vector<btl::future::Future<>> futures;
    for (auto& context : contexts)
    {
        futures.push_back(btl::async([&context]() mutable
            {

                EXPECT_EQ("hello world!", context.evaluate());

                auto r = context.update(FrameInfo(1, signal_time_t(16)));

                EXPECT_FALSE(r.didChange);
            }));
    }

    for (auto&& f : futures)
    {
        f.wait();
    }

    input.handle.set("bye");

    for (size_t i = 0; i < contexts.size(); ++i)
    {
        auto& f = futures[i];
        auto& context = contexts[i];

        std::move(f).then([&]()
            {
                EXPECT_EQ("hello world!", context.evaluate());

                auto r = context.update(FrameInfo(2, signal_time_t(16)));
                EXPECT_TRUE(r.didChange);

                EXPECT_EQ("bye world!", context.evaluate());
            });
    }

    for (auto&& f : futures)
    {
        f.wait();
    }
}

TEST(Signal2, join)
{
    auto input1 = makeInput<std::string>("hello");
    auto input2 = makeInput<std::string>("world");
    auto input3 = makeInput(42);
    auto input4 = makeInput<std::string>("!");
    auto input5 = makeInput(true);

    auto s1 = merge(input1.signal, input3.signal);
    auto s2 = merge(input2.signal, input3.signal);

    auto s = merge(input5.signal, input4.signal).map([s1, s2](bool v, std::string str)
        {
            return makeSignalResult(v ? s1 : s2, std::move(str));
        });

    auto j = s.join();

    auto c = makeSignalContext(j);

    auto r1 = c.evaluate();

    EXPECT_EQ("hello", r1.get<0>());
    EXPECT_EQ(42, r1.get<1>());
    EXPECT_EQ("!", r1.get<2>());

    input5.handle.set(false);
    input4.handle.set("?");

    c.update(FrameInfo(1, {}));
    auto r2 = c.evaluate();

    EXPECT_EQ("world", r2.get<0>());
    EXPECT_EQ(42, r2.get<1>());
    EXPECT_EQ("?", r1.get<2>());
}

TEST(Signal2, combine)
{
    std::vector<int> v1 = { 10, 20, 30, 40, 50 };
    std::vector<int> v2 = { 1, 2, 3, 4, 5 };

    std::vector<Input<SignalResult<int>, SignalResult<int>>> inputs;
    for (auto const& v : v1)
        inputs.push_back(makeInput(v));

    std::vector<AnySignal<int>> sigs;
    for (auto const& i : inputs)
        sigs.push_back(i.signal);

    auto s = combine(sigs);

    auto c = makeSignalContext(s);

    auto r = c.evaluate();

    EXPECT_EQ(v1, r);

    for (size_t i = 0; i < inputs.size(); ++i)
        inputs[i].handle.set(v2.at(i));

    auto ur = c.update(FrameInfo(1, {}));

    EXPECT_TRUE(ur.didChange);

    r = c.evaluate();

    EXPECT_EQ(v2, r);

    ur = c.update(FrameInfo(1, {}));
    EXPECT_FALSE(ur.didChange);
}

TEST(Signal2, conditional)
{
    auto cond = makeInput(true);
    auto input1 = makeInput<std::string, int>("hello", 42);
    auto input2 = makeInput<std::string, int>("world", 22);

    auto s = cond.signal.conditional(input1.signal, input2.signal);

    auto c = makeSignalContext(s);

    auto r = c.evaluate();

    EXPECT_EQ("hello", r.get<0>());
    EXPECT_EQ(42, r.get<1>());

    cond.handle.set(false);

    auto ur = c.update(FrameInfo(1, {}));
    r = c.evaluate();

    EXPECT_TRUE(ur.didChange);

    EXPECT_EQ("world", r.get<0>());
    EXPECT_EQ(22, r.get<1>());
}

TEST(Signal2, weak)
{
    auto input = makeInput(42);

    auto s = input.signal.share();

    auto w = s.weak();

    auto c2 = makeSignalContext(w);

    {
        auto c1 = makeSignalContext(std::move(s));

        EXPECT_EQ(42, c2.evaluate());

        input.handle.set(22);
        auto r = c2.update(FrameInfo(1, {}));

        EXPECT_TRUE(r.didChange);
        EXPECT_EQ(22, c2.evaluate());
    }

    input.handle.set(2);
    auto r = c2.update(FrameInfo(2, {}));
    EXPECT_FALSE(r.didChange);
    EXPECT_EQ(22, c2.evaluate());
}

TEST(Signal2, tee)
{
    auto input1 = makeInput<std::string, int>("hello", 42);
    auto input2 = makeInput<std::string, int>("world", 22);

    auto s1 = input1.signal.tee(input2.handle);

    auto s2 = merge(s1, input2.signal).map([](std::string const& s1, int i1,
                std::string const& s2, int i2)
        {
            return makeSignalResult(s1 + s2, i1 + i2);
        });

    auto c = makeSignalContext(s2);

    auto r1 = c.evaluate();

    EXPECT_EQ("hellohello", r1.get<0>());
    EXPECT_EQ(84, r1.get<1>());

    c.update(FrameInfo(1, {}));

    auto r2 = c.evaluate();

    EXPECT_EQ("hellohello", r2.get<0>());
    EXPECT_EQ(84, r2.get<1>());

    input1.handle.set("world", 22);
    c.update(FrameInfo(2, {}));

    auto r3 = c.evaluate();

    EXPECT_EQ("worldworld", r3.get<0>());
    EXPECT_EQ(44, r3.get<1>());
}

TEST(Signal2, teeCircular)
{
    auto input1 = makeInput<std::string, int>("hello", 42);
    auto input2 = makeInput<std::string, int>("world", 22);

    auto s1 = merge(input1.signal, input2.signal).map(
            [](std::string const& s1, int i1, std::string const& s2, int i2)
            {
                return makeSignalResult(s1 + s2, i1 + i2);
            });

    auto s2 = s1.tee(input2.handle);

    auto c = makeSignalContext(s2);

    auto r1 = c.evaluate();

    EXPECT_EQ("helloworld", r1.get<0>());
    EXPECT_EQ(64, r1.get<1>());

    c.update(FrameInfo(1, {}));

    auto r2 = c.evaluate();

    EXPECT_EQ("hellohelloworld", r2.get<0>());
    EXPECT_EQ(106, r2.get<1>());
}

TEST(Signal2, teeWithFunc)
{
    auto input1 = makeInput<std::string, int>("hello", 42);
    auto input2 = makeInput<std::string, int>("world", 22);

    auto s1 = merge(input1.signal, input2.signal).map(
            [](std::string const& s1, int i1, std::string const& s2, int i2)
            {
                return makeSignalResult(i1 + i2, s1 + s2);
            });

    auto s2 = s1.tee(input2.handle, [](int i, std::string s)
            {
                return makeSignalResult(s, i);
            });

    auto c = makeSignalContext(s2);

    auto r1 = c.evaluate();

    EXPECT_EQ("helloworld", r1.get<0>());
    EXPECT_EQ(64, r1.get<1>());

    c.update(FrameInfo(1, {}));

    auto r2 = c.evaluate();

    EXPECT_EQ("hellohelloworld", r2.get<0>());
    EXPECT_EQ(106, r2.get<1>());
}

TEST(Signal2, makeOptional)
{
    auto s = constant(42);

    auto s2 = s.makeOptional();

    auto c = makeSignalContext(s2);

    EXPECT_EQ(Type<std::optional<int> const&>(), getType(c.evaluate()));

    EXPECT_EQ(std::make_optional(42), c.evaluate());
}

TEST(Signal2, fromOptional)
{
    std::optional<AnySignal<int>> n = std::nullopt;

    auto s = fromOptional(n);

    EXPECT_EQ(Type<AnySignal<std::optional<int>>>(), Type<decltype(s)>());
}

TEST(Signal2, cache)
{
    auto input = makeInput(42);

    auto s1 = input.signal.map([](int i)
        {
            return std::to_string(i);
        });

    auto s2 = s1.cache();

    int callCount = 0;
    auto s3 = s2.map([&callCount](std::string const& s)
        {
            ++callCount;
            return s + s;
        });

    auto c = makeSignalContext(s3);

    EXPECT_EQ(1, callCount);

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ(1, callCount);

    input.handle.set(22);
    r = c.update(FrameInfo(2, {}));

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ(2, callCount);
}

TEST(Signal2, check)
{
    auto input = makeInput(42);

    auto s1 = input.signal.map([](int i)
        {
            return std::to_string(i);
        });

    auto s2 = s1.check();

    int callCount = 0;
    auto s3 = s2.map([&callCount](std::string const& s)
        {
            ++callCount;
            return s + s;
        });

    auto c = makeSignalContext(s3);

    EXPECT_EQ(1, callCount);

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ(1, callCount);

    input.handle.set(42);
    r = c.update(FrameInfo(2, {}));

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ(1, callCount);

    input.handle.set(22);
    r = c.update(FrameInfo(3, {}));

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ(2, callCount);
}

TEST(Signal2, cast)
{
    auto s1 = constant([](int i)
            {
                return std::to_string(i);
            });

    auto s2 = s1.cast<std::function<std::string(int)>>();

    auto c = makeSignalContext(s2);

    auto f = c.evaluate();

    EXPECT_EQ(Type<std::function<std::string(int)>>(), Type<decltype(f)>());

    EXPECT_EQ("20", f(20));
}
