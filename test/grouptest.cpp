#if 0
#include "reactive/signal/group.h"

#include "reactive/signal/constant.h"
#include "reactive/signal/signaltraits.h"

#include <gtest/gtest.h>
#include <tuple>

using namespace reactive::signal;

static_assert(IsSignal<Group<AnySignal<int>, AnySignal<std::string>>>::value, "");

TEST(SignalGroup, test)
{
    auto s1 = constant(10).map([](auto) { return 10; });
    auto s2 = group(std::move(s1), constant(std::string("test")));

    static_assert(std::is_same_v<
            decltype(s2.evaluate()),
            SignalResult<int, std::string const&>
            >, "");

    static_assert(IsSignalType<decltype(s2), int, std::string>::value, "");

    s2.evaluate();

    //AnySignal<int, std::string> s3 = std::move(s2);


    //static_assert(std::is_same<Group<Constant<int>, Constant<std::string>>, decltype(s)>::value, "");
}

#endif
