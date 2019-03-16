#pragma once

namespace reactive
{

    template <typename THint, typename TFunc1,
             typename TFunc2, typename TFunc3>
    struct MapSizeHint
    {
        SizeHintResult operator()() const
        {
            return func1(hint());
        }

        SizeHintResult operator()(float x) const
        {
            return func2(hint(x), x);
        }

        SizeHintResult operator()(float x, float y) const
        {
            return func3(hint(x, y), x, y);
        }

        std::decay_t<THint> hint;
        std::decay_t<TFunc1> func1;
        std::decay_t<TFunc2> func2;
        std::decay_t<TFunc3> func3;
    };

    template <typename THint, typename TFunc1,
             typename TFunc2, typename TFunc3,
             typename =
        std::enable_if_t<
            IsSizeHint<THint>::value
            && std::is_same<SizeHintResult,
                std::result_of_t<TFunc1(SizeHintResult)>
                >::value
            && std::is_same<SizeHintResult,
                std::result_of_t<TFunc2(SizeHintResult, float)>
                >::value
            && std::is_same<SizeHintResult,
                std::result_of_t<TFunc3(SizeHintResult, float, float)>
                >::value
            >
        >
    auto mapSizeHint(THint&& hint, TFunc1&& func1, TFunc2&& func2,
            TFunc3&& func3)
        -> MapSizeHint<
            std::decay_t<THint>,
            std::decay_t<TFunc1>,
            std::decay_t<TFunc2>,
            std::decay_t<TFunc3>
        >
    {
        return MapSizeHint<
            std::decay_t<THint>,
            std::decay_t<TFunc1>,
            std::decay_t<TFunc2>,
            std::decay_t<TFunc3>
            >
            {
                std::forward<THint>(hint),
                std::forward<TFunc1>(func1),
                std::forward<TFunc2>(func2),
                std::forward<TFunc3>(func3)
            };
    }
} // namespace reactive

