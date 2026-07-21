#include <bq/signal/sharedvector.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace bq::signal;

namespace
{
    struct Item
    {
        std::string key;
        int value;
    };

    auto itemKey = [](Item const& item)
    {
        return item.key;
    };
} // namespace

TEST(sharedVector, aVectorStartsWithWhatItWasGiven)
{
    SharedVector<int> empty;
    SharedVector<int> braced = { 1, 2, 3 };
    SharedVector<int> fromVector(std::vector<int>{ 4, 5 });

    EXPECT_EQ(std::vector<int>(), *empty.read());
    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), *braced.read());
    EXPECT_EQ((std::vector<int>{ 4, 5 }), *fromVector.read());

    auto c = makeSignalContext(braced.signal());

    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), c.evaluate<0>().get<0>());
}

// The write scope is the unit of emission, so the number of mutations under a
// handle is invisible downstream: what arrives is one change carrying the
// final state.
TEST(sharedVector, aWriteScopeIsOneEmission)
{
    SharedVector<int> vec;

    auto c = makeSignalContext(vec.signal());

    {
        auto w = vec.write();
        for (int i = 0; i < 100; ++i)
            w->push_back(i);
    }

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ(100u, c.evaluate<0>().get<0>().size());
    EXPECT_EQ(99, c.evaluate<0>().get<0>().back());

    r = c.update(FrameInfo(2, {}));

    EXPECT_FALSE(r.didChange);
}

// What publishes is the scope, not the mutation, so a scope that mutates
// nothing still emits. Stated as a test because the alternative — comparing
// against the previous contents — is a plausible thing to add later, and it
// would change this.
TEST(sharedVector, aWriteScopeThatMutatesNothingStillEmits)
{
    SharedVector<int> vec = { 1 };

    auto c = makeSignalContext(vec.signal());

    {
        auto w = vec.write();
        (void)w;
    }

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ(std::vector<int>{ 1 }, c.evaluate<0>().get<0>());
}

// Between two update passes a signal carries a level, not a history, so write
// scopes that both end before the next pass arrive as one change.
TEST(sharedVector, writeScopesBetweenTwoPassesCoalesce)
{
    SharedVector<int> vec;

    auto c = makeSignalContext(vec.signal());

    {
        auto w = vec.write();
        w->push_back(1);
    }

    {
        auto w = vec.write();
        w->push_back(2);
    }

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());
}

// Publication happens inside the write lock, so a read that begins after the
// scope ends can never observe contents the signal has not been given. What
// the signal has not yet *shown* is a separate thing: a context sees the new
// contents at its next update pass.
TEST(sharedVector, aReadNeverRunsAheadOfWhatTheSignalWasGiven)
{
    SharedVector<int> vec;

    auto c = makeSignalContext(vec.signal());

    {
        auto w = vec.write();
        w->push_back(1);
    }

    EXPECT_EQ(std::vector<int>{ 1 }, *vec.read());
    EXPECT_EQ(std::vector<int>(), c.evaluate<0>().get<0>());

    c.update(FrameInfo(1, {}));

    EXPECT_EQ(*vec.read(), c.evaluate<0>().get<0>());
}

TEST(sharedVector, copiesShareOneSetOfContents)
{
    SharedVector<int> first = { 1 };
    SharedVector<int> second = first;

    {
        auto w = second.write();
        w->push_back(2);
    }

    EXPECT_EQ((std::vector<int>{ 1, 2 }), *first.read());

    auto c = makeSignalContext(first.signal(), second.signal());

    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<1>().get<0>());
}

// A moved-into handle owns the scope, and the handle it came from must not
// publish a second time when it goes out of scope after it.
TEST(sharedVector, aMovedWriteHandlePublishesOnce)
{
    SharedVector<int> vec;

    auto c = makeSignalContext(vec.signal());

    {
        auto first = vec.write();
        first->push_back(1);

        auto second = std::move(first);
        second->push_back(2);
    }

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_TRUE(r.didChange);
    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());

    r = c.update(FrameInfo(2, {}));

    EXPECT_FALSE(r.didChange);
}

// A handle keeps the contents alive, so reading through one does not depend on
// a vector naming them still existing.
TEST(sharedVector, aHandleOutlivesTheVectorItCameFrom)
{
    auto vec = std::make_unique<SharedVector<int>>(std::vector<int>{ 1, 2 });

    auto r = vec->read();
    vec.reset();

    EXPECT_EQ((std::vector<int>{ 1, 2 }), *r);
}

// The signal is not a view onto the vector but a source in its own right, so
// it keeps working once nothing can write to it again.
TEST(sharedVector, theSignalOutlivesTheVector)
{
    AnySignal<std::vector<int>> sig = []()
        {
            SharedVector<int> vec = { 1, 2 };

            return vec.signal();
        }();

    auto c = makeSignalContext(sig);

    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_FALSE(r.didChange);
}

// The point of the whole thing: a vector drives forEach, and an item's value
// changing reaches the delegate through the signal it already holds rather
// than by rebuilding what it built.
TEST(sharedVector, feedsForEachAndBuildsOncePerKey)
{
    SharedVector<Item> items({ { "a", 1 }, { "b", 2 } });

    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(join(forEach(
                    items.signal(),
                    itemKey,
                    [builds](AnySignal<Item> item)
                    {
                        ++*builds;

                        return AnySignal<int>(item.map(
                                    [](Item const& i)
                                    {
                                        return i.value;
                                    }));
                    })));

    EXPECT_EQ(2, *builds);
    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());

    {
        auto w = items.write();
        w->push_back({ "c", 3 });
    }

    c.update(FrameInfo(1, {}));

    EXPECT_EQ(3, *builds);
    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), c.evaluate<0>().get<0>());

    {
        auto w = items.write();
        (*w)[0].value = 10;
    }

    c.update(FrameInfo(2, {}));

    EXPECT_EQ(3, *builds);
    EXPECT_EQ((std::vector<int>{ 10, 2, 3 }), c.evaluate<0>().get<0>());

    {
        auto w = items.write();
        w->erase(w->begin());
    }

    c.update(FrameInfo(3, {}));

    EXPECT_EQ(3, *builds);
    EXPECT_EQ((std::vector<int>{ 2, 3 }), c.evaluate<0>().get<0>());
}

// Readers run concurrently with each other and with nothing else. The
// invariant is that every element holds the same number, which only a reader
// admitted part-way through a write scope could see broken.
TEST(sharedVector, concurrentReadersNeverSeeAPartialWrite)
{
    int const elements = 64;
    int const writes = 200;

    SharedVector<int> vec(std::vector<int>(elements, 0));

    std::atomic<bool> stop(false);
    std::atomic<int> failures(0);

    std::vector<std::thread> readers;
    for (int i = 0; i < 2; ++i)
    {
        readers.emplace_back([&vec, &stop, &failures]()
            {
                while (!stop.load())
                {
                    {
                        auto r = vec.read();

                        for (int value : *r)
                        {
                            if (value != r->front())
                                ++failures;
                        }
                    }

                    std::this_thread::yield();
                }
            });
    }

    for (int i = 1; i <= writes; ++i)
    {
        auto w = vec.write();

        for (int& value : *w)
            value = i;
    }

    stop.store(true);

    for (auto& reader : readers)
        reader.join();

    EXPECT_EQ(0, failures.load());
    EXPECT_EQ(std::vector<int>(elements, writes), *vec.read());

    auto c = makeSignalContext(vec.signal());

    EXPECT_EQ(*vec.read(), c.evaluate<0>().get<0>());
}
