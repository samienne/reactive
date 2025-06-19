#pragma once

#define BTL_FN(fn) \
    [](auto&&... params) \
        -> decltype(fn(std::forward<decltype(params)>(params)...)) \
    { \
        return fn(std::forward<decltype(params)>(params)...); \
    }

#define BTL_MEM_FN(fn) [](auto&& obj, auto&&... params) { return std::forward<decltype(obj)>(obj).fn(std::forward<decltype(params)>(params)...); }
