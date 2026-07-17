#pragma once

#include <btl/future/future.h>
#include <btl/future/futurecontrol.h>
#include <btl/future/promise.h>

#include <memory>
#include <utility>

// Test-only helper: create a manually-completed promise/future pair over a
// shared FutureControl, mirroring what btl::async() does internally but WITHOUT
// the threadpool. This lets tests drive completion explicitly instead of using
// wall-clock sleeps as an ordering proxy.
//
// The returned Promise holds a weak_ptr to the control, so the Future (which
// owns the shared control) must stay alive for the Promise to be usable.
//
//   auto [p, fut] = makeManualFuture<int>();
//   ...
//   p.set(42);                       // completes fut deterministically
//   EXPECT_EQ(42, std::move(fut).get());
namespace btl::test
{
    template <typename... Ts>
    std::pair<future::Promise<Ts...>, future::Future<Ts...>> makeManualFuture()
    {
        auto control = std::make_shared<future::FutureControl<Ts...>>();
        future::Promise<Ts...> promise(control);
        future::Future<Ts...> future(std::move(control));

        return { std::move(promise), std::move(future) };
    }
} // namespace btl::test
