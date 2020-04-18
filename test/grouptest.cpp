#include "reactive/signal/group.h"

#include "reactive/signal/constant.h"
#include "reactive/signal/signaltraits.h"

#include <gtest/gtest.h>
#include <tuple>

using namespace reactive::signal;

static_assert(IsSignal<Group<AnySignal<int>, AnySignal<std::string>>>::value, "");

TEST(SignalGroup, test)
{
    auto ff = []() { return std::string("test"); };

    auto s = group(constant(10).map([](auto) { return 10; }), constant(std::string("test")));

    auto as = s.evaluate();
    //as = 1;

    /*
    static_assert(std::is_same_v<
            decltype(s.evaluate()),
            SignalResult<int const&, std::string const&>
            >, "");

    static_assert(IsSignalType<decltype(s), int, std::string>::value, "");
    */

    auto r = s.evaluate();

    //AnySignal<int, std::string> s = std::move(s);


    //static_assert(std::is_same<Group<Constant<int>, Constant<std::string>>, decltype(s)>::value, "");
}

