#include <reactive/widget/instance.h>

#include <gtest/gtest.h>

using namespace reactive::widget;

static_assert(std::is_copy_constructible<Instance>::value, "");
static_assert(std::is_copy_assignable<Instance>::value, "");

