#include <reactive/widget.h>

#include <gtest/gtest.h>

using namespace reactive;

static_assert(std::is_copy_constructible<Widget>::value, "");
static_assert(std::is_copy_assignable<Widget>::value, "");

