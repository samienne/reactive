#pragma once

#include "avgvisibility.h"

#include <btl/all.h>
#include <btl/not.h>

#include <memory>
#include <typeinfo>

namespace avg
{
    class AVG_EXPORT CurveBase
    {
    public:
        virtual ~CurveBase() = default;
        virtual float call(float t) const = 0;
        virtual std::type_info const& getTypeInfo() const = 0;
        virtual bool compare(CurveBase const& rhs) const = 0;
    };

    template <typename T>
    class CurveTyped : public CurveBase
    {
    public:
        CurveTyped(T t) :
            t_(std::move(t))
        {
        }

        float call(float t) const override
        {
            return t_(t);
        }

        std::type_info const& getTypeInfo() const override
        {
            return typeid(T);
        }

        bool compare(CurveBase const& rhs) const override
        {
            return reinterpret_cast<CurveTyped<T> const&>(rhs).t_ == t_;
        }

    private:
        T t_;
    };

    class AVG_EXPORT Curve
    {
    public:
        template <typename TFunc, typename = std::enable_if_t<
            btl::All<
                std::is_invocable_r<float, TFunc, float>,
                btl::Not<std::is_same<std::decay_t<TFunc>, Curve>>
            >::value>>
        Curve(TFunc&& func) :
            ptr_(std::make_shared<CurveTyped<std::decay_t<TFunc>>>(
                        std::forward<TFunc>(func)
                        )
                )
        {
        }

        Curve(std::shared_ptr<CurveBase> base) :
            ptr_(std::move(base))
        {
        }

        Curve(Curve const&) = default;
        Curve(Curve&&) noexcept = default;

        Curve& operator=(Curve const&) = default;
        Curve& operator=(Curve&&) noexcept = default;

        float operator()(float t) const
        {
            return ptr_->call(t);
        };

        bool operator==(Curve const& rhs) const noexcept
        {
            if (ptr_->getTypeInfo() != rhs.ptr_->getTypeInfo())
                return false;

            return ptr_->compare(*rhs.ptr_);
        }

        bool operator!=(Curve const& rhs) const noexcept
        {
            return !(*this == rhs);
        }

    private:
        std::shared_ptr<CurveBase> ptr_;
    };
} // namespace avg
