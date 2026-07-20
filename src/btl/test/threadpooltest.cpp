#include <btl/threadpool.h>

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

using btl::TaskQueue;
using btl::TimedQueue;

namespace
{
    auto record(std::vector<int>& order, int id)
    {
        return [&order, id]
        {
            order.push_back(id);
        };
    }
} // anonymous namespace

TEST(timedQueue, popsSingleTask)
{
    auto now = TimedQueue::Clock::now();
    std::vector<int> order;

    TimedQueue queue;
    EXPECT_TRUE(queue.empty());

    queue.push(now + std::chrono::milliseconds(10), record(order, 1));

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(now + std::chrono::milliseconds(10), queue.getNextEventTime());

    queue.pop()();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(std::vector<int>({1}), order);
}

TEST(timedQueue, popsInTimeOrder)
{
    auto now = TimedQueue::Clock::now();
    std::vector<int> order;

    TimedQueue queue;
    queue.push(now + std::chrono::milliseconds(40), record(order, 4));
    queue.push(now + std::chrono::milliseconds(10), record(order, 1));
    queue.push(now + std::chrono::milliseconds(50), record(order, 5));
    queue.push(now + std::chrono::milliseconds(20), record(order, 2));
    queue.push(now + std::chrono::milliseconds(30), record(order, 3));

    for (int i = 0; i != 5; ++i)
    {
        EXPECT_EQ(
                now + std::chrono::milliseconds(10 * (i + 1)),
                queue.getNextEventTime()
                );

        queue.pop()();
    }

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(std::vector<int>({1, 2, 3, 4, 5}), order);
}

TEST(timedQueue, reportsReadinessOfEarliestTask)
{
    auto now = TimedQueue::Clock::now();
    std::vector<int> order;

    TimedQueue queue;
    EXPECT_FALSE(queue.hasTaskReady());

    queue.push(now + std::chrono::hours(1), record(order, 2));
    EXPECT_FALSE(queue.hasTaskReady());

    queue.push(now - std::chrono::hours(1), record(order, 1));
    EXPECT_TRUE(queue.hasTaskReady());

    queue.pop()();
    EXPECT_FALSE(queue.hasTaskReady());
    EXPECT_EQ(std::vector<int>({1}), order);
}

TEST(taskQueue, popsDueTimedTasksInTimeOrder)
{
    auto now = TimedQueue::Clock::now();
    std::vector<int> order;

    TaskQueue queue;
    queue.pushTimed(now - std::chrono::milliseconds(10), record(order, 3));
    queue.pushTimed(now - std::chrono::milliseconds(30), record(order, 1));
    queue.pushTimed(now - std::chrono::milliseconds(20), record(order, 2));

    for (int i = 0; i != 3; ++i)
    {
        auto task = queue.pop();
        ASSERT_TRUE(task.has_value());
        std::move(*task)();
    }

    EXPECT_EQ(std::vector<int>({1, 2, 3}), order);
}

TEST(taskQueue, popsUntimedTasksWhenNoTimedTaskIsDue)
{
    auto now = TimedQueue::Clock::now();
    std::vector<int> order;

    TaskQueue queue;
    queue.pushTimed(now - std::chrono::hours(1), record(order, 1));
    queue.push(record(order, 2));
    queue.push(record(order, 3));
    queue.pushTimed(now + std::chrono::hours(1), record(order, 4));

    for (int i = 0; i != 3; ++i)
    {
        auto task = queue.pop();
        ASSERT_TRUE(task.has_value());
        std::move(*task)();
    }

    EXPECT_EQ(std::vector<int>({1, 2, 3}), order);
}
