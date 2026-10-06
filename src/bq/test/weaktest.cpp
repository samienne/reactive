#include <bq/signal/weak.h>
#include <bq/signal/datacontext.h>

#include <gtest/gtest.h>

#include <memory>

using namespace bq::signal;

namespace
{
    class CountingControl : public SharedControlBase<int>
    {
    public:
        struct Data : BaseDataType
        {
        };

        std::shared_ptr<BaseDataType> baseInitialize(DataContext&,
                FrameInfo const&) override
        {
            return std::make_shared<Data>();
        }

        SignalResult<int const&> baseEvaluate(DataContext&,
                BaseDataType const&) override
        {
            ++evaluateCount;
            return SignalResult<int const&>(value);
        }

        UpdateResult baseUpdate(DataContext&, BaseDataType&,
                FrameInfo const&) override
        {
            ++updateCount;
            ++value;
            return { true };
        }

        int value = 0;
        int evaluateCount = 0;
        int updateCount = 0;
    };
} // anonymous namespace

TEST(weak, updatesSourceOncePerFrame)
{
    auto control = std::make_shared<CountingControl>();
    Weak<int> weak(control);

    DataContext context;
    auto data = weak.initialize(context, FrameInfo(0, {}));
    int evaluatesAfterInit = control->evaluateCount;

    FrameInfo frame(1, {});
    auto first = weak.update(context, data, frame);
    auto second = weak.update(context, data, frame);

    EXPECT_EQ(1, control->updateCount);
    EXPECT_EQ(evaluatesAfterInit + 1, control->evaluateCount);
    EXPECT_TRUE(first.didChange);
    EXPECT_TRUE(second.didChange);
    EXPECT_EQ(1, weak.evaluate(context, data)->get<0>());
}

TEST(weak, sourceExpiredBeforeInitialize)
{
    auto control = std::make_shared<CountingControl>();
    Weak<int> weak(control);
    control.reset();

    DataContext context;
    auto data = weak.initialize(context, FrameInfo(0, {}));

    auto result = weak.update(context, data, FrameInfo(1, {}));

    EXPECT_FALSE(result.didChange);
    EXPECT_FALSE(weak.evaluate(context, data).has_value());
}
