#include <btl/invoke.h>

using namespace btl;

static_assert(std::is_same_v<
        std::tuple<std::string, int>,
        ParamPackApplyT<std::tuple, ParamPack<std::string, int>>
        >);

static_assert(std::is_same_v<
    ParamPack<std::string, int>,
    ConcatParamPacksT<ParamPack<std::string>, ParamPack<int>>
    >);

static_assert(std::is_same_v<
    ParamPack<>,
    ConcatParamPacksT<>
    >);

static_assert(isInvocableRV<int, std::function<int(std::string)>, std::string>);
static_assert(isInvocableRV<int, std::function<int()>, ParamPack<>>);
static_assert(!isInvocableRV<int, std::function<int(int)>, std::string>);

static_assert(isInvocableRV<int, std::function<int(std::string)>,
        UnpackTupleT<std::tuple<std::string>>>);
static_assert(isInvocableRV<int, std::function<int(std::string, int, int)>,
        UnpackTupleT<std::tuple<std::string, int>>, int>);
