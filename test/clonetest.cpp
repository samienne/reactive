#include <btl/cloneoncopy.h>

#include <gtest/gtest.h>

class Clonable
{
public:
    Clonable(int data) :
        data(data)
    {
    }

    Clonable(Clonable&&) = default;

    Clonable& operator=(Clonable&&) = default;

    Clonable clone() const
    {
        return *this;
    };

    int getData() const
    {
        return data;
    }

private:
    Clonable(Clonable const&) = default;
    Clonable& operator=(Clonable const&) = default;

private:

    int data;
};


class NotClonable
{
public:
    NotClonable()
    {
    }

    NotClonable(NotClonable&&) = default;

    NotClonable& operator=(NotClonable&&) = default;

private:
    NotClonable(NotClonable const&) = default;
    NotClonable& operator=(NotClonable const&) = default;
};

static_assert(btl::IsClonable<Clonable>::value, "");
static_assert(!btl::IsClonable<NotClonable>::value, "");
static_assert(!std::is_copy_constructible<NotClonable>::value, "");

TEST(CloneOnCopy, copy)
{
    btl::option<Clonable> c = btl::just(btl::clone(Clonable(10)));
    btl::option<std::string> ss = btl::just<char const*>("test");

    auto v = btl::cloneOnCopy(Clonable(10));

    auto v2 = v;

    EXPECT_EQ(10, v->getData());
    EXPECT_EQ(10, v2->getData());
}

