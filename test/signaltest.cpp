#include <reactive/signal/cache.h>
#include <reactive/signal/convert.h>
#include <reactive/signal/weak.h>
#include <reactive/signal/split.h>
#include <reactive/signal/combine.h>
#include <reactive/signal/waitfor.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/update.h>
#include <reactive/signal/removereference.h>
#include <reactive/sharedsignal.h>
#include <reactive/signal.h>

#include <btl/delayed.h>
#include <btl/always.h>

#include "signaltester.h"
#include <gtest/gtest.h>

#include <iostream>
#include <string>

using namespace reactive;

TEST(signal, cacheTest)
{
    Signal<std::string> s = signal::constant<std::string>("test");

    auto s2 = signal::cache(std::move(s));
}

TEST(signal, construct)
{
    auto s1 = signal::wrap(signal::constant<std::string>("test"));
    Signal<std::string> s2 = std::move(s1);
    Signal<std::string> s3 = std::move(s2);

    EXPECT_EQ("test", s3.evaluate());

    auto s4 = s3.clone();

    auto s5 = signal::wrap(signal::constant(true));

    static_assert(IsSignal<decltype(s1)>::value, "");
    static_assert(IsSignal<decltype(s2)>::value, "");
    static_assert(IsSignal<decltype(s3)>::value, "");
    static_assert(IsSignal<decltype(s4)>::value, "");
    static_assert(IsSignal<decltype(s5)>::value, "");
}

TEST(signal, combine)
{
    static_assert(IsSignal<
            signal::Combine<std::vector<SharedSignal<const int&>>>
            >::value, "CombineSignal is not a signal");

    static_assert(IsSignal<
            signal::Combine<std::tuple<SharedSignal<int>>>
            >::value, "CombineSignal is not a signal");
}

TEST(signal, clone)
{
    auto s1 = signal::wrap(signal::constant<std::string>("test"));
    Signal<std::string> s2 = s1.clone();
    auto s3 = s2.clone();

    auto s4 = signal::wrap(s3.clone());

    EXPECT_EQ("test", s3.evaluate());
}

TEST(signal, sharedCopy)
{
    auto s1 = signal::wrap(signal::constant<std::string>("test"));
    auto s2 = signal::share(std::move(s1));
    auto s3 = s2;
    Signal<std::string> s4 = s3;

    auto s5 = signal::share(s3);

    static_assert(std::is_same<decltype(s2), decltype(s3)>::value, "");
    static_assert(std::is_same<decltype(s2), decltype(s5)>::value, "");

    static_assert(IsSignal<decltype(s2)>::value, "");
    static_assert(IsSignal<decltype(s2)>::value, "");
    static_assert(IsSignal<decltype(s3)>::value, "");
    static_assert(IsSignal<decltype(s4)>::value, "");
    static_assert(IsSignal<decltype(s5)>::value, "");

    EXPECT_EQ("test", s5.evaluate());
}

TEST(signal, waitFor)
{
    auto f = btl::delayed(std::chrono::seconds(1), btl::always(1));
    auto s = reactive::signal::waitFor(10, std::move(f));

    EXPECT_EQ(10, s.evaluate());

    std::this_thread::sleep_for(std::chrono::seconds(2));

    signal::update(s, reactive::signal::FrameInfo(1u, std::chrono::seconds(2)));

    EXPECT_TRUE(s.hasChanged());
    EXPECT_EQ(1, s.evaluate());
}

TEST(signal, split)
{
    auto s = signal::constant(std::make_tuple(10, std::string("20")));

    auto tuple = signal::split(std::move(s));

    auto s1 = std::move(std::get<0>(tuple));
    auto s2 = std::move(std::get<1>(tuple));

    EXPECT_EQ(10, s1.evaluate());
    EXPECT_EQ(std::string("20"), s2.evaluate());

    Signal<int> s3 = std::move(s1);
    Signal<std::string> s4 = std::move(s2);

    EXPECT_EQ(10, s3.evaluate());
    EXPECT_EQ(std::string("20"), s4.evaluate());
}

TEST(signal, cacheOptimizations)
{
    auto make = []()
    {
        return signal::cache(signal::removeReference(
                    signal::constant(std::string("test"))
                    ));
    };

    auto s1 = signal::cache(make());

    auto s2 = signal::cache(std::move(s1));

    static_assert(std::is_same<decltype(s1), decltype(s2)>::value, "");
}

TEST(signal, cacheOptimizations2)
{
    auto s1 = signal::cache(signal::constant<std::string>("test"));

    static_assert(std::is_same<
            std::decay_t<decltype(s1.signal())>,
            signal::Constant<std::string>>::value,
            "");
}

TEST(signal, shareOptimizations)
{
    auto make = []()
    {
        return signal::share(signal::constant(std::string("test")));
    };

    auto s = make();

    auto s1 = signal::share(make());

    auto s2 = signal::share(s1);

    static_assert(std::is_same<decltype(s1), decltype(s1)>::value, "");
}

template <typename T, typename U>
auto asdf(Signal<T, U> s)
{
    return std::move(s);
}

template <typename T>
auto asdf2(Signal<T> s)
{
    return std::move(s);
}

TEST(signal, typedSharedSignalIsASignal)
{
    //SharedSignal<int const&, signal::Cache<signal::Constant<int>>>
    auto s1 = signal::share(signal::constant(10));
    auto s2 = asdf(s1);

    auto s3 = asdf2<int>(s1);

    //Signal<int const&, signal::Constant<int>> s3 = s1;

    s2.evaluate();
}

TEST(signal, sharedSignalTypeReduction)
{
    auto s1 = signal::share(signal::constant(10));
    SharedSignal<int> s2 = s1;

    s2.evaluate();
}

TEST(signal, sharedSignalTypeReductionToSignal)
{
    auto s1 = share(signal::constant(10));
    Signal<int> s2 = s1;

    s2.evaluate();
}

TEST(signal, sharedSignalIsASignal)
{
    auto s1 = share(signal::constant(10));
    Signal<int> s2 = s1;

    s2.evaluate();
}

TEST(signal, weakConstruct)
{
    SharedSignal<int> s1 = signal::share(signal::constant(10));
    auto s2 = signal::weak(s1);

    s2.evaluate();
}

TEST(signal, Convert)
{
    auto s1 = signal::constant([](){});
    signal::Convert<std::function<void()>> s2(std::move(s1));

    s2.evaluate();
}

TEST(signal, tester)
{
    testSignal([](auto s)
    {
        return signal::cache(std::move(s));
    });

    testSignal([](auto s)
    {
        return signal::share(std::move(s));
    });
}

