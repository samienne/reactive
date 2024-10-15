#include <reactive/signal/signal.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/signalcontext.h>
#include <reactive/signal/input.h>
#include <reactive/signal/merge.h>
#include <reactive/signal/join.h>
#include <reactive/signal/combine.h>
#include <reactive/signal/fromoptional.h>
#include <reactive/signal/evaluateoninit.h>

#include <btl/demangle.h>

#include <gtest/gtest.h>

using namespace reactive::signal;

static_assert(std::is_same_v<
        ConcatSignalResults<SignalResult<int, char>, SignalResult<int const&, std::string&>>::type,
        SignalResult<int, char, int const&, std::string&>
        >);

static_assert(checkSignal<Constant<int>>());
static_assert(checkSignal<Combine<int>>());
static_assert(checkSignal<Conditional<Constant<bool>, int>>());
static_assert(checkSignal<EvaluateOnInit<std::function<int()>>>());
static_assert(checkSignal<InputSignal<int>>());
static_assert(checkSignal<Join<Constant<Constant<int>>>>());
static_assert(checkSignal<Map<std::function<int(int)>, Constant<int>>>());
static_assert(checkSignal<Merge<Constant<int>>>());
static_assert(checkSignal<Shared<Constant<int>, int>>());
static_assert(checkSignal<WithChanged<Constant<int>>>());
static_assert(checkSignal<WithPrevious<Constant<int>, std::function<int(int, int)>>>());

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

TEST(signal, constant)
{
    auto s = constant(10);

    static_assert(checkSignal<decltype(s.unwrap())>());

    auto c = makeSignalContext(s);

    EXPECT_EQ(10, c.evaluate());

    EXPECT_EQ(Type<int const&>(), getType(c.evaluate()));

    FrameInfo frame(1, signal_time_t(10));

    auto r = c.update(frame);
    EXPECT_EQ(r.nextUpdate, std::nullopt);
}

TEST(signal, signalContext)
{
    auto c = makeSignalContext(constant(42));

    UpdateResult r = c.update(FrameInfo(1, signal_time_t(0)));

    EXPECT_EQ(std::nullopt, r.nextUpdate);

    EXPECT_EQ(Type<int const&>(), getType(c.evaluate()));

    int v = c.evaluate();

    EXPECT_EQ(42, v);
}

TEST(signal, signalInput)
{
    auto input = makeInput(42);

    static_assert(checkSignal<decltype(input.signal.unwrap())>());

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

TEST(signal, map)
{
    auto input = makeInput(42);

    auto s = input.signal.map([](int n)
                {
                    return n * 2;
                });

    static_assert(checkSignal<decltype(s.unwrap())>());

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

TEST(signal, mapReferenceToTemp)
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

    static_assert(checkSignal<decltype(s1.unwrap())>());
    static_assert(checkSignal<decltype(s2.unwrap())>());

    auto c = makeSignalContext(std::move(s2));

    EXPECT_EQ(42, c.evaluate());
}

TEST(signal, merge)
{
    auto input1 = makeInput(42);
    auto input2 = makeInput<int, std::string>(20, "test");

    auto s = merge(input1.signal, input2.signal);

    static_assert(checkSignal<decltype(s.unwrap())>());

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

TEST(signal, share)
{
    auto input = makeInput<std::string>("hello");

    auto s1 = input.signal.map([](std::string const& str)
            {
                return str + " world!";
            });

    auto s2 = s1.share();

    static_assert(checkSignal<decltype(s2.unwrap())>());

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

TEST(signal, join)
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

    static_assert(checkSignal<decltype(j.unwrap())>());

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

TEST(signal, combine)
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

    static_assert(checkSignal<decltype(s.unwrap())>());

    auto c = makeSignalContext(s);

    auto r = c.evaluate();

    EXPECT_EQ(v1, r);

    for (size_t i = 0; i < inputs.size(); ++i)
        inputs[i].handle.set(v2.at(i));

    auto ur = c.update(FrameInfo(1, {}));

    EXPECT_TRUE(ur.didChange);

    r = c.evaluate();

    EXPECT_EQ(v2, r);

    ur = c.update(FrameInfo(2, {}));
    EXPECT_FALSE(ur.didChange);
}

TEST(signal, conditional)
{
    auto cond = makeInput(true);
    auto input1 = makeInput<std::string, int>("hello", 42);
    auto input2 = makeInput<std::string, int>("world", 22);

    auto s = cond.signal.conditional(input1.signal, input2.signal);

    static_assert(checkSignal<decltype(s.unwrap())>());

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

/*
TEST(signal, weak)
{
    auto input = makeInput(42);

    auto s = input.signal.share();

    auto w = s.weak();

    static_assert(checkSignal<decltype(w.unwrap())>());

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
*/

TEST(signal, tee)
{
    auto input1 = makeInput<std::string, int>("hello", 42);
    auto input2 = makeInput<std::string, int>("world", 22);

    auto s1 = input1.signal.tee(input2.handle);

    static_assert(checkSignal<decltype(s1.unwrap())>());

    auto s2 = s1.merge(input2.signal).map([](std::string const& s1, int i1,
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

TEST(signal, teeCircular)
{
    auto input1 = makeInput<std::string, int>("hello", 42);
    auto input2 = makeInput<std::string, int>("world", 22);

    auto s1 = merge(input1.signal, input2.signal).map(
            [](std::string const& s1, int i1, std::string const& s2, int i2)
            {
                return makeSignalResult(s1 + s2, i1 + i2);
            });

    auto s2 = s1.tee(input2.handle);

    static_assert(checkSignal<decltype(s2.unwrap())>());

    auto c = makeSignalContext(s2);

    auto r1 = c.evaluate();

    EXPECT_EQ("hellohelloworld", r1.get<0>());
    EXPECT_EQ(106, r1.get<1>());

    c.update(FrameInfo(1, {}));

    auto r2 = c.evaluate();

    EXPECT_EQ("hellohelloworld", r2.get<0>());
    EXPECT_EQ(106, r2.get<1>());
}

TEST(signal, teeWithFunc)
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

    static_assert(checkSignal<decltype(s2.unwrap())>());

    auto c = makeSignalContext(s2);

    auto r1 = c.evaluate();

    EXPECT_EQ(106, r1.get<0>());
    EXPECT_EQ("hellohelloworld", r1.get<1>());

    c.update(FrameInfo(1, {}));

    auto r2 = c.evaluate();

    EXPECT_EQ(106, r2.get<0>());
    EXPECT_EQ("hellohelloworld", r2.get<1>());
}

TEST(signal, makeOptional)
{
    auto s = constant(42);

    auto s2 = s.makeOptional();

    auto c = makeSignalContext(s2);

    EXPECT_EQ(Type<std::optional<int> const&>(), getType(c.evaluate()));

    EXPECT_EQ(std::make_optional(42), c.evaluate());
}

TEST(signal, fromOptional)
{
    std::optional<AnySignal<int>> n = std::nullopt;

    auto s = fromOptional(n);

    EXPECT_EQ(Type<AnySignal<std::optional<int>>>(), Type<decltype(s)>());
}

TEST(signal, cache)
{
    auto input = makeInput(42);

    auto s1 = input.signal.map([](int i)
        {
            return std::to_string(i);
        });

    auto s2 = s1.cache();

    static_assert(checkSignal<decltype(s2.unwrap())>());

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

TEST(signal, check)
{
    auto input = makeInput(42);

    auto s1 = input.signal.map([](int i)
        {
            return std::to_string(i);
        });

    auto s2 = s1.check();

    static_assert(checkSignal<decltype(s2.unwrap())>());

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

TEST(signal, cast)
{
    auto s1 = constant([](int i)
            {
                return std::to_string(i);
            });

    auto s2 = s1.cast<std::function<std::string(int)>>();

    static_assert(checkSignal<decltype(s2.unwrap())>());

    auto c = makeSignalContext(s2);

    auto f = c.evaluate();

    EXPECT_EQ(Type<std::function<std::string(int)>>(), Type<decltype(f)>());

    EXPECT_EQ("20", f(20));
}

TEST(signal, bindToFunction)
{
    auto input1 = makeInput(42, std::string("hello"));

    auto s1 = input1.signal.bindToFunction([](int i, std::string const& s1,
                std::string const& s2)
            {
                return std::to_string(i) + s1 + ", " + s2;
            });

    static_assert(checkSignal<decltype(s1.unwrap())>());

    AnySignal<std::function<std::string(std::string)>> s2 = s1;

    static_assert(checkSignal<decltype(s2.unwrap())>());

    auto c = makeSignalContext(s2);

    auto f = c.evaluate();

    EXPECT_EQ("42hello, world", f("world"));
}

TEST(signal, withChanged)
{
    auto input = makeInput(42, std::string("hello"));

    auto s1 = input.signal.withChanged();

    static_assert(checkSignal<decltype(s1.unwrap())>());

    auto c = makeSignalContext(s1);

    auto v = c.evaluate();
    EXPECT_FALSE(v.get<0>());
    EXPECT_EQ(42, v.get<1>());
    EXPECT_EQ("hello", v.get<2>());

    auto r = c.update(FrameInfo(1, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_FALSE(v.get<0>());
    EXPECT_EQ(42, v.get<1>());
    EXPECT_EQ("hello", v.get<2>());

    input.handle.set(22, "world");

    r = c.update(FrameInfo(2, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_TRUE(v.get<0>());
    EXPECT_EQ(22, v.get<1>());
    EXPECT_EQ("world", v.get<2>());

    r = c.update(FrameInfo(3, {}));
    v = c.evaluate();

    // Inner value did not change but our own first value changed from true to
    // false
    EXPECT_TRUE(r.didChange);
    EXPECT_FALSE(v.get<0>());
    EXPECT_EQ(22, v.get<1>());
    EXPECT_EQ("world", v.get<2>());

    r = c.update(FrameInfo(4, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_FALSE(v.get<0>());
    EXPECT_EQ(22, v.get<1>());
    EXPECT_EQ("world", v.get<2>());
}

TEST(signal, withPrevious)
{
    auto input = makeInput(std::string("world"));

    auto s = input.signal.withPrevious(
            [](std::string prevS, std::string s)
            {
                return prevS + s;
            },
            std::string("hello"));

    static_assert(checkSignal<decltype(s.unwrap())>());

    auto c = makeSignalContext(s);

    auto v = c.evaluate();

    EXPECT_EQ("helloworld", v);

    auto r = c.update(FrameInfo(1, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("helloworld", v);

    input.handle.set("bye");
    r = c.update(FrameInfo(2, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("helloworldbye", v);

    r = c.update(FrameInfo(3, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("helloworldbye", v);
}

TEST(signal, withPreviousMulti)
{
    auto input = makeInput(42, std::string("hello"));

    auto s = input.signal.withPrevious(
            [](std::string prevS, int prevI, int i, std::string s)
            {
                return makeSignalResult(s + prevS, i + prevI);
            },
            std::string("world"), 22);

    static_assert(checkSignal<decltype(s.unwrap())>());

    auto c = makeSignalContext(s);

    auto v = c.evaluate();

    EXPECT_EQ("helloworld", v.get<0>());
    EXPECT_EQ(64, v.get<1>());

    auto r = c.update(FrameInfo(1, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("helloworld", v.get<0>());
    EXPECT_EQ(64, v.get<1>());

    input.handle.set(33, "bye");
    r = c.update(FrameInfo(2, {}));
    v = c.evaluate();

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ("byehelloworld", v.get<0>());
    EXPECT_EQ(97, v.get<1>());

    r = c.update(FrameInfo(3, {}));
    v = c.evaluate();

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ("byehelloworld", v.get<0>());
    EXPECT_EQ(97, v.get<1>());
}
