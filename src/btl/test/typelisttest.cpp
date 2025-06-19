#include <btl/typelist.h>

#include <string>

using namespace btl;

static_assert(std::is_same<
        Difference<
            TypeList<std::string, int, std::string>,
            TypeList<int>
        >::type,
        TypeList<std::string, std::string>
        >::value,
        "");

static_assert(std::is_same<
        Intersection<
            TypeList<std::string, int, std::string>,
            TypeList<int>
        >::type,
        TypeList<int>
        >::value,
        "");

static_assert(std::is_same<
        Unique<TypeList<std::string, int, std::string>>::type,
        TypeList<int, std::string>
        >::value,
        "");

static_assert(std::is_same<
        Filter<std::is_integral, TypeList<std::string, int, std::string>>::type,
        TypeList<int>
        >::value,
        "");

