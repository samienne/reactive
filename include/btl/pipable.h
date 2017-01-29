#pragma once

#include <fit/lambda.h>
#include <fit/pipable.h>

#define BTL_PIPABLE(fn) \
    fit::pipable( \
                FIT_STATIC_LAMBDA(auto&&... ts) \
                    -> decltype( \
                        fn(std::forward<decltype(ts)>(ts)...) \
                    ) \
                { \
                    return fn(std::forward<decltype(ts)>(ts)...); \
                } \
            )

