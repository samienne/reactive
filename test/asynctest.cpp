#include <btl/future/just.h>
#include <btl/future/merge.h>
#include <btl/future/mbind.h>
#include <btl/future/fmap.h>
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
        sleep(1);
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

TEST(async, fmap)
{
    auto f = [](int a, int b)
    {
        return a + b;
    };

    auto g = []()
    {
        return 20;
    };

    auto future = fmap(f, async(g), async(g));

    EXPECT_EQ(40, std::move(future).get());
}

TEST(async, fmap2)
{
    auto f = fmap2(makeFuture(std::string("test")), makeFuture(3),
            [](std::string const& s, int n)
            {
                std::string r;
                for (int i = 0; i < n; ++i)
                    r += s;
                return r;
            });

    EXPECT_EQ(std::string("testtesttest"), std::move(f).get());
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

TEST(async, fmapWithRef)
{
    auto s = makeSharedFuture(std::make_shared<int>(20));
    Future<std::shared_ptr<int> const&> f = s;

    auto f2 = fmap([](std::shared_ptr<int> const& p) -> bool
        {
            return p != nullptr;
        },
        std::move(f)
        );

    EXPECT_TRUE(std::move(f2).get());
}

TEST(async, futureMemberFMap)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = std::move(f).fmap([](std::unique_ptr<int>)
        {
            return true;
        });

    EXPECT_TRUE(std::move(f2).get());
}

TEST(async, futureMemberFMapMultiArg)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = makeFuture(std::make_unique<int>(40));
    auto f3 = std::move(f).fmap([](std::unique_ptr<int>, std::unique_ptr<int>)
        {
            return true;
        }, std::move(f2));

    EXPECT_TRUE(std::move(f3).get());
}

TEST(async, sharedFutureMemberFMap)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = std::move(f).fmap([](std::shared_ptr<int>)
        {
            return true;
        });

    EXPECT_TRUE(std::move(f2).get());
}

TEST(async, sharedFutureMemberFMapMultiArg)
{
    auto f = makeFuture(std::make_shared<int>(20));
    auto f2 = makeFuture(std::make_shared<int>(40));
    auto f3 = std::move(f).fmap([](std::shared_ptr<int>, std::shared_ptr<int>)
        {
            return true;
        }, std::move(f2));

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

        auto f2 = fmap([&didRun](std::shared_ptr<int>)
            {
                didRun = true;
                return false;
            }, std::move(f));
    }

    // Make sure that the future has time to finish if it is running
    sleep(2);

    EXPECT_FALSE(didRun);
}

TEST(async, futureListen)
{
    std::atomic<bool> didCall(false);

    auto f = makeFuture(std::make_unique<int>(20));
    auto c = f.connect();
    std::move(f).listen([&didCall](std::unique_ptr<int>)
        {
            didCall = true;
        });

    sleep(2);

    EXPECT_TRUE(didCall);
}

TEST(async, futureListenCancel)
{
    std::atomic<bool> didCall(false);

    {
        auto f = makeFuture(std::make_unique<int>(20));
        auto c = f.connect();
        std::move(f).listen([&didCall](std::unique_ptr<int>)
            {
                didCall = true;
            });
    }

    sleep(2);

    EXPECT_FALSE(didCall);
}

TEST(async, sharedFutureListen)
{
    std::atomic<bool> didCall(false);

    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto c = f.connect();
    f.listen([&didCall](std::shared_ptr<int>)
        {
            didCall = true;
        });

    sleep(2);

    EXPECT_TRUE(didCall);
}

TEST(async, sharedFutureListenCancel)
{
    std::atomic<bool> didCall(false);

    {
        auto f = makeSharedFuture(std::make_shared<int>(20));
        auto c = f.connect();
        f.listen([&didCall](std::shared_ptr<int>)
            {
                didCall = true;
            });
    }

    sleep(2);

    EXPECT_FALSE(didCall);
}

TEST(async, fmapMember)
{
    btl::MoveOnlyFunction<size_t(std::vector<int> const&)>
        mof(&std::vector<int>::size);

    auto f = makeFuture(std::vector<int>({10, 20, 30}));
    auto f2 = std::move(f)
        .fmap(&std::vector<int>::size);

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
    auto f2 = mbind([](std::unique_ptr<int>)
        {
            return makeFuture(std::make_unique<std::string>("test"));
        }, std::move(f));

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, futureMemberMbind)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = std::move(f).mbind([](std::unique_ptr<int>)
        {
            return makeFuture(std::make_unique<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, futureMemberMbindMultiArg)
{
    auto f = makeFuture(std::make_unique<int>(20));
    auto f2 = makeFuture(std::make_unique<int>(20));
    auto f3 = std::move(f).mbind([](std::unique_ptr<int>, std::unique_ptr<int>)
        {
            return makeFuture(std::make_unique<std::string>("test"));
        }, std::move(f2));

    EXPECT_EQ(std::string("test"), *std::move(f3).get());
}

TEST(async, sharedFutureMbind)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = mbind([](std::shared_ptr<int>)
        {
            return makeSharedFuture(std::make_shared<std::string>("test"));
        }, f);

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, sharedFutureMemberMbind)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = f.mbind([](std::shared_ptr<int>)
        {
            return makeSharedFuture(std::make_shared<std::string>("test"));
        });

    EXPECT_EQ(std::string("test"), *std::move(f2).get());
}

TEST(async, sharedFutureMemberMbindMultiArg)
{
    auto f = makeSharedFuture(std::make_shared<int>(20));
    auto f2 = makeSharedFuture(std::make_shared<int>(20));
    auto f3 = f.mbind([](std::shared_ptr<int>, std::shared_ptr<int>)
        {
            return makeSharedFuture(std::make_shared<std::string>("test"));
        }, f2);

    EXPECT_EQ(std::string("test"), *std::move(f3).get());
}

TEST(async, perf)
{
    std::uniform_int_distribution<int> dist(1, 12);

    size_t const count = 100000;
    size_t const steps = 20;

    {
        sleep(1);
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
            auto fut = async(std::bind(f, delta));
            accs.push_back(std::move(fut));
        }

        auto acc = fmap([](std::vector<uint64_t> const& accs)
            {
                return btl::reduce(0ul, accs, btl::Plus());
            },
            merge(std::move(accs))
            );

        std::cout << std::move(acc).get() << std::endl;

        auto stop = std::chrono::steady_clock::now();

        std::chrono::duration<float> t = stop - start;

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

        std::cout << "Unthreaded took: " << t.count() << std::endl;
    }
}

TEST(async, delayed)
{
    auto f = btl::delayed(std::chrono::seconds(2), btl::always(1));

    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(std::move(f).get());


    auto delay = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start);
    EXPECT_GE(delay.count(), 2.f);
}

TEST(async, just)
{
    auto f = btl::delayed(std::chrono::seconds(1), btl::always(1));

    auto f2 = btl::future::just(std::move(f));

    EXPECT_TRUE(std::move(f2).get().valid());
    EXPECT_EQ(1, *std::move(f2).get());
}

