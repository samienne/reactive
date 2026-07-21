#include <bq/signal/datacontext.h>

#include <gtest/gtest.h>

#include <memory>

using namespace bq::signal;

// DataContext keeps weak references, so an id whose data has been released
// still has an entry in the map. Initializing that id again is the normal way
// a node that comes back after everything dropped it gets its state.
TEST(dataContext, expiredIdCanBeInitializedAgain)
{
    DataContext context;
    auto id = makeUniqueId();

    {
        auto data = context.initializeData<int>(id, 42);
        EXPECT_EQ(42, *data);
        EXPECT_EQ(42, *context.findData<int>(id));
    }

    EXPECT_EQ(nullptr, context.findData<int>(id));

    auto data = context.initializeData<int>(id, 7);
    EXPECT_EQ(7, *data);
    EXPECT_EQ(7, *context.findData<int>(id));
}

// Two ids are independent, and re-initializing an expired one leaves the other
// alone.
TEST(dataContext, expiryOfOneIdLeavesOthersAlone)
{
    DataContext context;
    auto first = makeUniqueId();
    auto second = makeUniqueId();

    auto kept = context.initializeData<int>(first, 1);

    {
        auto data = context.initializeData<int>(second, 2);
        EXPECT_EQ(2, *data);
    }

    context.initializeData<int>(second, 3);

    EXPECT_EQ(1, *context.findData<int>(first));
    EXPECT_EQ(1, *kept);
}
