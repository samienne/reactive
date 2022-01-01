#include <reactive/signal/blip.h>
#include <reactive/signal/conditional.h>
#include <reactive/signal/count.h>
#include <reactive/signal/delay.h>
#include <reactive/signal/changed.h>
#include <reactive/signal/droprepeats.h>
#include <reactive/signal/dt.h>
#include <reactive/signal/updateifjust.h>
#include <reactive/signal/onchange.h>
#include <reactive/signal/every.h>
#include <reactive/signal/combine.h>
#include <reactive/signal/join.h>
#include <reactive/signal/cache.h>
#include <reactive/signal/convert.h>
#include <reactive/signal/weak.h>
#include <reactive/signal/split.h>
#include <reactive/signal/combine.h>
#include <reactive/signal/waitfor.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/update.h>
#include <reactive/signal/removereference.h>
#include <reactive/signal/sharedsignal.h>
#include <reactive/signal/signal.h>

#include <btl/delayed.h>
#include <btl/always.h>

#include "signaltester.h"
#include <gtest/gtest.h>

#include <iostream>
#include <string>

using namespace reactive::signal;

template <typename TSignal>
struct TestSignal : std::conditional_t<
                    CheckSignal<TSignal>::value == IsSignal<TSignal>::value,
                    std::true_type,
                    std::false_type>
{
};

template <typename T>
struct RequireSignal : TestSignal<T>
{
    static_assert(IsSignal<T>::value, "");
    static_assert(CheckSignal<T>::value, "");
    static_assert(std::is_same<bool, hasChanged_t<T>>::value, "");
    static_assert(std::is_same<UpdateResult, updateBegin_t<T>>::value, "");
    static_assert(std::is_same<UpdateResult, updateEnd_t<T>>::value, "");
    static_assert(std::is_same<reactive::Connection, observe_t<T>>::value, "");
    static_assert(std::is_same<reactive::Annotation, annotate_t<T>>::value, "");
    static_assert(std::is_nothrow_move_constructible<std::decay_t<T>>::value, "");
    static_assert(btl::IsClonable<std::decay_t<T>>::value, "");
};

int add(int a, int b)
{
    return a + b;
}

static_assert(RequireSignal<Constant<int>>::value, "");
static_assert(RequireSignal<decltype(
            map(add, constant(10), constant(20))
            )>::value, "");
static_assert(RequireSignal<Join<Constant<Constant<int>>>>::value,
        "");
static_assert(RequireSignal<Combine<std::vector<AnySignal<int>>>>::value,
        "");
static_assert(RequireSignal<Cache<Constant<int>>>::value, "");
static_assert(RequireSignal<InputSignal<int, btl::DummyLock>>::value,
    "InputSignal is not a signal");
static_assert(RequireSignal<Every>::value, "Every is not a signal");
static_assert(RequireSignal<
        OnChange<Constant<int>, btl::Function<void()>>>::value, "");
static_assert(RequireSignal<UpdateIfJust<Constant<btl::option<int>>>>::value, "");
static_assert(RequireSignal<Changed<Constant<int>>>::value,
        "Changed is not a signal");
static_assert(RequireSignal<Blip<Constant<int>>>::value, "");
static_assert(RequireSignal<AnySharedSignal<int>>::value, "");
static_assert(IsSignal<Delay<int const&, Constant<int>>>::value, "");
static_assert(IsSignal<Conditional<Constant<bool>, Constant<int>,
        Constant<int>>>::value, "");
static_assert(IsSignal<CountSignal<Constant<int>>>::value,
        "Count is not a signal");
static_assert(IsSignal<DropRepeats<Constant<int>>>::value,
        "DropRepeatsSignal is not a signal");
static_assert(IsSignal<DtSignal>::value, "DtSignal is not a signal");

TEST(signal, cacheTest)
{
    AnySignal<std::string> s = constant<std::string>("test");

    auto s2 = cache(std::move(s));
}

TEST(signal, construct)
{
    auto s1 = wrap(constant<std::string>("test"));
    AnySignal<std::string> s2 = std::move(s1);
    AnySignal<std::string> s3 = std::move(s2);

    EXPECT_EQ("test", s3.evaluate());

    auto s4 = s3.clone();

    auto s5 = wrap(constant(true));

    static_assert(IsSignal<decltype(s1)>::value, "");
    static_assert(IsSignal<decltype(s2)>::value, "");
    static_assert(IsSignal<decltype(s3)>::value, "");
    static_assert(IsSignal<decltype(s4)>::value, "");
    static_assert(IsSignal<decltype(s5)>::value, "");
}

TEST(signal, combine)
{
    static_assert(IsSignal<
            Combine<std::vector<AnySharedSignal<const int&>>>
            >::value, "CombineSignal is not a signal");

    static_assert(IsSignal<
            Combine<std::tuple<AnySharedSignal<int>>>
            >::value, "CombineSignal is not a signal");
}

TEST(signal, clone)
{
    auto s1 = wrap(constant<std::string>("test"));
    AnySignal<std::string> s2 = s1.clone();
    auto s3 = s2.clone();

    auto s4 = wrap(s3.clone());

    EXPECT_EQ("test", s3.evaluate());
}

TEST(signal, sharedCopy)
{
    auto s1 = wrap(constant<std::string>("test"));
    auto s2 = share(std::move(s1));
    auto s3 = s2;
    AnySignal<std::string> s4 = s3;

    auto s5 = share(s3);

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
    auto s = waitFor(10, std::move(f));

    EXPECT_EQ(10, s.evaluate());

    std::this_thread::sleep_for(std::chrono::seconds(2));

    update(s, FrameInfo(1u, std::chrono::seconds(2)));

    EXPECT_TRUE(s.hasChanged());
    EXPECT_EQ(1, s.evaluate());
}

TEST(signal, split)
{
    auto s = constant(std::make_tuple(10, std::string("20")));

    auto tuple = split(std::move(s));

    auto s1 = std::move(std::get<0>(tuple));
    auto s2 = std::move(std::get<1>(tuple));

    EXPECT_EQ(10, s1.evaluate());
    EXPECT_EQ(std::string("20"), s2.evaluate());

    AnySignal<int> s3 = std::move(s1);
    AnySignal<std::string> s4 = std::move(s2);

    EXPECT_EQ(10, s3.evaluate());
    EXPECT_EQ(std::string("20"), s4.evaluate());
}

TEST(signal, cacheOptimizations)
{
    auto make = []()
    {
        return cache(removeReference(
                    constant(std::string("test"))
                    ));
    };

    auto s1 = cache(make());

    auto s2 = cache(std::move(s1));

    static_assert(std::is_same<decltype(s1), decltype(s2)>::value, "");
}

TEST(signal, cacheOptimizations2)
{
    auto s1 = cache(constant<std::string>("test"));

    static_assert(std::is_same<
            std::decay_t<decltype(s1.storage())>,
            Constant<std::string>>::value,
            "");
}

TEST(signal, shareOptimizations)
{
    auto make = []()
    {
        return share(constant(std::string("test")));
    };

    auto s = make();

    auto s1 = share(make());

    auto s2 = share(s1);

    static_assert(std::is_same<decltype(s1), decltype(s1)>::value, "");
}

template <typename T, typename U>
auto asdf(Signal<T, U> s)
{
    return s;
}

template <typename T>
auto asdf2(AnySignal<T> s)
{
    return s;
}

TEST(signal, typedSharedSignalIsASignal)
{
    //SharedSignal<int const&, signal::Cache<signal::Constant<int>>>
    auto s1 = share(constant(10));
    auto s2 = asdf(s1);

    auto s3 = asdf2<int>(s1);

    //Signal<int const&, signal::Constant<int>> s3 = s1;

    s2.evaluate();
}

TEST(signal, sharedSignalTypeReduction)
{
    auto s1 = share(constant(10));
    AnySharedSignal<int> s2 = s1;

    s2.evaluate();
}

TEST(signal, sharedSignalTypeReductionToSignal)
{
    auto s1 = share(constant(10));
    AnySignal<int> s2 = s1;

    s2.evaluate();
}

TEST(signal, sharedSignalIsASignal)
{
    auto s1 = share(constant(10));
    AnySignal<int> s2 = s1;

    s2.evaluate();
}

TEST(signal, weakConstruct)
{
    AnySharedSignal<int> s1 = share(constant(10));
    auto s2 = weak(s1);

    s2.evaluate();
}

TEST(signal, Convert)
{
    auto s1 = constant([](){});
    Convert<std::function<void()>> s2(std::move(s1));

    s2.evaluate();
}

TEST(signal, tester)
{
    testSignal([](auto s)
    {
        return cache(std::move(s));
    });

    testSignal([](auto s)
    {
        return share(std::move(s));
    });
}

