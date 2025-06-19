#include <btl/fmap.h>

#include <gtest/gtest.h>

using namespace btl;

static_assert(IsFunctor<std::vector<int>>::value, "");
static_assert(IsFunctor<std::tuple<int, int>>::value, "");
static_assert(!IsFunctor<int>::value, "");

