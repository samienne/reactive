#include <atomic>
#include <btl/future/join.h>
#include <btl/future/merge.h>
#include <btl/future/whenall.h>
#include <btl/future/sharedfuture.h>
#include <btl/future/future.h>

#include <btl/delayed.h>
#include <btl/async.h>
#include <btl/reduce.h>
#include <btl/plus.h>
#include <btl/always.h>

#include <gtest/gtest.h>

#include <random>
#include <chrono>
#include <thread>
#include <type_traits>

using namespace btl::future;
using btl::async;
using btl::ThreadPool;

static_assert(std::is_same<
        int,
        decltype(std::declval<Future<int>>().get())
        >::value, "");

static_assert(std::is_same<
        int const&,
        decltype(std::declval<Future<const int&>>().get())
        >::value, "");

template <typename T>
auto makeFuture(T&& value)
{
    return async([value=std::forward<T>(value)]() mutable
        -> std::decay_t<T>
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return std::forward<T>(value);
    });
}

template <typename T>
auto makeSharedFuture(T&& value)
{
    return makeFuture(std::forward<T>(value))
        .share();
}

TEST(async, run)
{
    auto f = [] { return 20; };

    std::vector<Future<int>> r;

    for (int i = 0; i < 20000; ++i)
        r.push_back(async(f));

    for (int i = 0; i < 20000; ++i)
        EXPECT_EQ(20, std::move(r[i]).get());
}

static_assert(std::is_same_v<
        std::string,
        FutureValueTypeT<Future<Future<Future<std::string>>>>
        >);

TEST(async, then)
{
    auto f = async([]() { return 10; });

    auto f2 = std::move(f).then([](int i)
            {
                return std::to_string(i);
            });

    auto f3 = std::move(f2).then([](std::string s)
            {
                return async([s]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));

                    return "test: " + s;
                });
            });

    EXPECT_EQ("test: 10", std::move(f3).get());
}

TEST(async, whenAll)
{
    auto f = whenAll(
            async([]() { return 10; }),
            async([]() { return 20; })
            );

    auto f2 = std::move(f).then([](int i, int j)
            {
                return i + j;
            });

    EXPECT_EQ(30, std::move(f2).get());
}

TEST(async, futureShare)
{
    auto s = makeSharedFuture(std::make_unique<int>(20));

    int count = 100;
    std::vector<SharedFuture<std::unique_ptr<int> const&>> futures;
    for (int i = 0; i < count; ++i)
        futures.push_back(s);

    for (auto&& future : futures)
        EXPECT_EQ(20, *future.get());
}

TEST(async, sharedFutureCastToFuture)
{
    auto s = makeSharedFuture(std::make_unique<int>(20));

    int count = 100;
    std::vector<Future<std::unique_ptr<int> const&>> futures;
    for (int i = 0; i < count; ++i)
        futures.push_back(s);

    for (auto&& future : futures)
        EXPECT_EQ(20, *std::move(future).get());

    EXPECT_EQ(20, *s.get());
}

TEST(async, futureConstRefToFuture)
{
    auto s = makeSharedFuture(std::make_shared<int>(20));
    Future<std::shared_ptr<int> const&> f = s;

    Future<std::shared_ptr<int>> f2 = std::move(f);

    EXPECT_EQ(20, *std::move(f2).get());
}

TEST(async, futureToConstRefFuture)
{
    auto f = makeFuture(std::make_shared<int>(20));
    Future<std::shared_ptr<int> const&> f2 = std::move(f);

    EXPECT_EQ(20, *std::move(f2).get());
}

TEST(async, thenWithRef)
{
    auto s = makeSharedFuture(std::make_shared<int>(20));
    Future<std::shared_ptr<int> const&> f = s;

    auto f2 = std::move(f)
        .then([](std::shared_ptr<int> const& p) -> bool
        {
            return p != nullptr;
        });

    EXPECT_TRUE(std::move(f2).get());
}

TEST(async, whenAllMulti)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = makeFuture(std::make_unique<int>(40));
    auto f3 = whenAll(std::move(f), std::move(f2))
        .then([](std::unique_ptr<int>, std::unique_ptr<int>)
        {
            return true;
        });

    EXPECT_TRUE(std::move(f3).get());
}

TEST(async, sharedFutureMemberThen)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = std::move(f).then([](std::shared_ptr<int>)
        {
            return true;
        });

    EXPECT_TRUE(std::move(f2).get());
}

TEST(async, sharedFutureMemberThenMultiArg)
{
    auto f = makeFuture(std::make_shared<int>(20));
    auto f2 = makeFuture(std::make_shared<int>(40));
    auto f3 = whenAll(std::move(f), std::move(f2))
        .then([](std::shared_ptr<int>, std::shared_ptr<int>)
        {
            return true;
        });

    EXPECT_TRUE(std::move(f3).get());
}

TEST(async, makeReadyFuture)
{
    auto p = std::make_shared<int>(20);
    auto f = makeReadyFuture(p);

    EXPECT_TRUE(f.ready());
    EXPECT_EQ(p, std::move(f).get());
}

TEST(async, autoCancel)
{
    std::atomic<bool> didRun(false);

    {
        auto f = makeFuture(std::make_shared<int>(20));

        auto f2 = std::move(f).then([&didRun](std::shared_ptr<int>)
            {
                didRun = true;
                return false;
            });
    }

    // Make sure that the future has time to finish if it is running
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_FALSE(didRun);
}

TEST(async, thenMember)
{
    auto f = makeFuture(std::vector<int>({10, 20, 30}));
    auto f2 = std::move(f)
        .then(&std::vector<int>::size);

    EXPECT_EQ(3, std::move(f2).get());
}

TEST(async, futureJoin)
{
    auto ff = makeFuture(makeFuture(std::make_unique<int>(20)));
    auto f = join(std::move(ff));
    EXPECT_EQ(20, *std::move(f).get());
}

TEST(async, sharedFutureJoin)
{
    auto ff = makeFuture(makeSharedFuture(std::make_shared<int>(20)));

    auto f = join(std::move(ff));
    EXPECT_EQ(20, *std::move(f).get());
}

TEST(async, futureMbind)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = std::move(f).then([](std::unique_ptr<int>)
        {
            return makeFuture(std::make_unique<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, futureMemberMbind)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = std::move(f).then([](std::unique_ptr<int>)
        {
            return makeFuture(std::make_unique<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, futureMemberMbindMultiArg)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = makeFuture(std::make_unique<int>(20));
    auto f3 = whenAll(std::move(f), std::move(f2))
        .then([](std::unique_ptr<int>, std::unique_ptr<int>)
        {
            return makeFuture(std::make_unique<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f3).get());
}

TEST(async, sharedFutureMbind)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = f.then([](std::shared_ptr<int>)
        {
            return makeSharedFuture(std::make_shared<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, sharedFutureMemberMbind)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = f.then([](std::shared_ptr<int>)
        {
            return makeSharedFuture(std::make_shared<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, sharedFutureMemberMbindMultiArg)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = makeSharedFuture(std::make_shared<int>(20));
    auto f3 = whenAll(f, f2).then([](std::shared_ptr<int>, std::shared_ptr<int>)
        {
            return makeSharedFuture(std::make_shared<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f3).get());
}

TEST(async, perf)
{
    std::uniform_int_distribution<int> dist(1, 12);

    size_t const count = 100000;
    size_t const steps = 32;

    std::chrono::duration<float> threadedTime;

    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "start threaded" << std::endl;
        auto start = std::chrono::steady_clock::now();

        auto f = [](size_t count)
        {
            std::default_random_engine engine(0);
            std::uniform_int_distribution<int> dist(1, 12);
            uint64_t acc = 0;
            for (size_t i = 0; i < count; ++i)
            {
                acc += dist(engine);
            }

            return acc;
        };

        std::vector<Future<uint64_t>> accs;
        int delta = count / steps;
        for (size_t i = 0; i < steps; ++i)
        {
            auto fut = btl::async(std::bind(f, delta));
            accs.push_back(std::move(fut));
        }

        auto acc = merge(std::move(accs)).then([](std::vector<uint64_t> const& accs)
            {
                return btl::reduce(0ul, accs, btl::Plus());
            });

        std::cout << std::move(acc).get() << std::endl;

        auto stop = std::chrono::steady_clock::now();

        std::chrono::duration<float> t = stop - start;
        threadedTime = t;

        std::cout << "Threaded took: " << t.count() << std::endl;
    }

    {
        std::cout << "start" << std::endl;
        auto start = std::chrono::steady_clock::now();

        uint64_t acc = 0;
        uint64_t delta = count / steps;
        for (size_t n = 0; n < steps; ++n)
        {
            std::default_random_engine engine(0);
            std::uniform_int_distribution<int> dist(1, 12);
            for (uint64_t i = 0; i < delta; ++i)
                acc += dist(engine);
        }

        std::cout << acc << std::endl;

        auto stop = std::chrono::steady_clock::now();

        std::chrono::duration<float> t = stop - start;

        std::cout << "Unthreaded took: " << t.count() << ". "
            << 100.0f * t.count() / threadedTime.count()
            << "% speedup for threaded." << std::endl;

        std::cout << "Threads: " << std::thread::hardware_concurrency() << ". "
            << (100.0f * t.count() / threadedTime.count()
                    / (float)std::thread::hardware_concurrency())
            << "% utilization per thread."
            << std::endl;
    }
}

TEST(async, delayed)
{
    auto f = btl::delayed(std::chrono::milliseconds(200), btl::always(1));

    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(std::move(f).get());


    auto delay = std::chrono::duration_cast<std::chrono::duration<float>>(
            std::chrono::steady_clock::now() - start);
    EXPECT_GE(delay.count(), 0.199f);
}

TEST(async, futureResult)
{
    auto f = btl::async([]()
            {
                return makeFutureResult(true, 20);
            }).then([](bool, int)
            {
                return makeFutureResult(10, std::string("test"), 20);
            });

    auto r = std::move(f).getTuple();

    EXPECT_EQ(10, std::get<0>(r));
    EXPECT_EQ(std::string("test"), std::get<1>(r));
    EXPECT_EQ(20, std::get<2>(r));
}

TEST(async, futureFail)
{
    bool failCalled = false;
    auto f = btl::async([]()
            {
                throw std::runtime_error("test error");

                return 10;
            })
            .onFailure([&](std::exception_ptr const&)
            {
                failCalled = true;
            });


    EXPECT_THROW(std::move(f).get(), std::runtime_error);
    EXPECT_TRUE(failCalled);
}

TEST(async, whenAllFail)
{
    for (int i = 0; i < 20000; ++i)
    {
        auto f1 = btl::async([]()
                {
                    throw std::runtime_error("test error");

                    return 10;
                });

        auto f2 = btl::async([]()
                {
                    return 20;
                });

        std::atomic_bool failCalled = false;

        auto r1 = whenAll(std::move(f1), std::move(f2))
            .then([](int x, int y)
            {
                return x+y;
            })
            .onFailure([&](std::exception_ptr const&)
            {
                failCalled.store(true);
            })
            ;

        EXPECT_THROW(std::move(r1).get(), std::runtime_error);
        EXPECT_TRUE(failCalled.load());
    }
}

TEST(async, whenAllCancelOnFail)
{
    for (int i = 0; i < 100; ++i)
    {
        auto f1 = btl::async([]()
                {
                    throw std::runtime_error("test error");

                    return 10;
                });

        std::atomic_bool called = false;

        auto f2 = btl::async([]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    return true;
                })
                .then([&called](bool)
                {
                    called.store(true);
                })
                ;

        auto r = whenAll(std::move(f1), std::move(f2));

        EXPECT_THROW(std::move(r).get();, std::runtime_error);
        EXPECT_FALSE(called.load());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TEST(async, mergeFail)
{
    for (int n = 0; n < 2000; ++n)
    {
        std::vector<Future<int>> v;
        v.reserve(2000);

        for (int i = 0; i < 200; ++i)
        {
            v.push_back(btl::async([i]()
                {
                    if (i % 3)
                        throw std::runtime_error("Test error");

                    return i;
                }));
        }

        auto r = merge(std::move(v));

        EXPECT_THROW(std::move(r).get(), std::runtime_error);
    }
}

