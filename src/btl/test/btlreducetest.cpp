#include <btl/reduce.h>

using namespace btl;

static_assert(IsReducable<std::tuple<int, int>>::value, "");
static_assert(IsReducable<std::vector<int>>::value, "");
static_assert(!IsReducable<int>::value, "");

