#if 0
#include <reactive/signal/tween.h>
#include <reactive/signal/input.h>

#include <gtest/gtest.h>

#include <chrono>

using namespace reactive;
using namespace reactive::signal;
using std::chrono::microseconds;
using std::chrono::seconds;

TEST(tweensignal, basic)
{
    auto i = signal::input<int>(10);
    auto t = signal::tween(seconds(1), 0.0f, std::move(i.signal));

    using Type = std::decay_t<decltype(t)>;

    static_assert(!std::is_copy_constructible<Type>::value, "");
    static_assert(btl::IsClonable<Type>::value, "");
    static_assert(IsSignal<Type>::value, "");
}

#endif
