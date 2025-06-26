#pragma once

#include <functional>
#include <type_traits>

namespace btl
{
    template <class HashAlgorithm>
    struct uhash
    {
        using result_type = typename HashAlgorithm::result_type;

        template <class T>
        result_type operator()(T const& t) const noexcept
        {
            HashAlgorithm h;
            hash_append(h, t);
            return static_cast<result_type>(h);
        }
    };

    template <class ResultType>
    class deferred_hash
    {
    public:
        using result_type = ResultType;

    private:
        using function = std::function<void(void const*, std::size_t)>;

        function hasher_;
        result_type (*convert_)(function&);

    public:
        /*template <class HashAlgorithm,
                 class = typename std::enable_if
                     <
                     std::is_constructible<function, HashAlgorithm>{} &&
                     std::is_same<typename std::decay<HashAlgorithm>::type::result_type,
                 result_type>{}
        >::type
            >
            explicit
            deferred_hash(HashAlgorithm&& h)
            : hasher_(std::forward<HashAlgorithm>(h))
              , convert_(convert<std::decay<HashAlgorithm>::type>)
        {
        }*/

        void operator()(void const* key, std::size_t len)
        {
            hasher_(key, len);
        }

        explicit operator result_type() noexcept
        {
            return convert_(hasher_);
        }

        /*template <class T>
        T* target() noexcept
        {
            return hasher_.target<T>();
        }*/

    private:
        /*template <class HashAlgorithm>
        static result_type
        convert(function& f) noexcept
        {
            return static_cast<result_type>(*f.target<HashAlgorithm>());;
        }*/
    };
}

