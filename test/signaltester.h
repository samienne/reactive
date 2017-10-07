#pragma once

#include <reactive/signal/updateresult.h>
#include <reactive/annotation.h>
#include <reactive/signaltraits.h>
#include <reactive/signal2.h>

#include <gtest/gtest.h>

#include <btl/shared.h>

class SignalStub
{
    struct Data
    {
        size_t evaluateCount_ = 0;
        size_t hasChangedCount_ = 0;
        size_t updateBeginCount_ = 0;
        size_t updateEndCount_ = 0;
    };

public:
    SignalStub() :
        data_(std::make_shared<Data>())
    {
    }

    int evaluate() const
    {
        ++data_->evaluateCount_;
        return 10;
    }

    bool hasChanged() const
    {
        ++data_->hasChangedCount_;
        return true;
    }

    reactive::signal::UpdateResult updateBegin(reactive::signal::FrameInfo
            const&)
    {
        ++data_->updateBeginCount_;
        return btl::none;
    }

    reactive::signal::UpdateResult updateEnd(reactive::signal::FrameInfo
            const&)
    {
        ++data_->updateEndCount_;
        return btl::none;
    }

    template <typename TFunc>
    reactive::Connection observe(TFunc&&)
    {
        return reactive::Connection();
    }

    reactive::Annotation annotate() const
    {
        reactive::Annotation a;
        return a;
    }

    size_t getEvaluateCount() const
    {
        return data_->evaluateCount_;
    }

    size_t getHasChangedCount() const
    {
        return data_->hasChangedCount_;
    }

    size_t getUpdateBeginCount() const
    {
        return data_->updateBeginCount_;
    }

    size_t getUpdateEndCount() const
    {
        return data_->updateEndCount_;
    }

private:
    mutable btl::shared<Data> data_;
};

static_assert(reactive::IsSignal<SignalStub>::value, "");

template <typename TSignalMap>
void testSignal(TSignalMap&& sigMap)
{
    SignalStub stub;

    auto s = sigMap(reactive::signal2::wrap(SignalStub(stub)));

    EXPECT_EQ(0u, stub.getEvaluateCount());
    EXPECT_EQ(0u, stub.getHasChangedCount());
    EXPECT_EQ(0u, stub.getUpdateBeginCount());
    EXPECT_EQ(0u, stub.getUpdateEndCount());

    reactive::signal::FrameInfo frame{1, std::chrono::microseconds(0)};

    s.updateBegin(frame);

    EXPECT_EQ(0u, stub.getEvaluateCount());
    EXPECT_EQ(0u, stub.getHasChangedCount());
    EXPECT_EQ(1u, stub.getUpdateBeginCount());
    EXPECT_EQ(0u, stub.getUpdateEndCount());

    s.updateEnd(frame);

    EXPECT_EQ(1u, stub.getUpdateEndCount());

    size_t evaluateCount = stub.getEvaluateCount();
    size_t hasChangedCount = stub.getHasChangedCount();
    size_t updateBeginCount_ = stub.getUpdateBeginCount();
    size_t updateEndCount = stub.getUpdateEndCount();

    reactive::signal::FrameInfo frame2{2, std::chrono::microseconds(0)};
    s.updateBegin(frame2);

    EXPECT_EQ(evaluateCount, stub.getEvaluateCount());
    EXPECT_EQ(hasChangedCount, stub.getHasChangedCount());
    EXPECT_EQ(updateBeginCount_ + 1, stub.getUpdateBeginCount());
    EXPECT_EQ(updateEndCount, stub.getUpdateEndCount());
}

