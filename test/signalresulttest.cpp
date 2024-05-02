#include "signaltester.h"
#if 0

#include <reactive/signal/signalresult.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace reactive::signal;

static_assert(IsSignalResultType<SignalResult<int>, int>::value, "");
static_assert(IsSignalResultType<SignalResult<long>, int>::value, "");
static_assert(!IsSignalResultType<SignalResult<int, std::string>, int>::value, "");
static_assert(!IsSignalResultType<SignalResult<int, std::string>, std::string, int>::value, "");

static_assert(IsSignalResultType<SignalResult<long, char*>, int, std::string>::value, "");

class CloneOnly
{
public:
    CloneOnly(int n) :
        n_(n)
    {
    };

    CloneOnly(CloneOnly const&) = delete;
    CloneOnly(CloneOnly&&) = default;

    CloneOnly& operator=(CloneOnly const&) = delete;
    CloneOnly& operator=(CloneOnly&&) = delete;

    int get() const
    {
        return n_;
    }

private:
    int n_ = 0;
};

TEST(SignalResult, test1)
{
    SignalResult<int> result(1);

    EXPECT_EQ(1, get<0>(result));

    CloneOnly o(42);
    SignalResult<CloneOnly> r(std::move(o));

    SignalResult<CloneOnly> r2 = std::move(r);

    EXPECT_EQ(42, get<0>(r2).get());
}

#endif
