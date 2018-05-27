#pragma once

#include "vector.h"

#include <btl/forcenoexcept.h>
#include <btl/visibility.h>

#include <cmath>
#include <ostream>

namespace avg
{
    /**
     * @brief Represents transform
     *
     * Transform vectors v_output = T * S * R * v, where T is the translation,
     * S is the scale, R is the rotation, v is the input vector and
     * v_output is the transformed vector.
     */
    class BTL_VISIBLE Transform final
    {
    public:
        inline Transform(Vector2f translation = Vector2f(0.0f, 0.0f),
                float scale = 1.0f, float rotation = 0.0f);
        Transform(Transform const&) = default;
        Transform(Transform&&) noexcept = default;
        inline ~Transform();

        Transform& operator=(Transform const&) = default;
        Transform& operator=(Transform&&) noexcept = default;

        inline Transform&& scale(float scale) &&;
        inline Transform&& rotate(float angle) &&;
        inline Transform&& translate(Vector2f v) &&;
        inline Transform&& translate(float x, float y) &&;

        inline Transform&& setScale(float scale) &&;
        inline Transform&& setRotation(float rotation) &&;
        inline Transform&& setTranslation(Vector2f v) &&;
        inline Transform&& setTranslation(float x, float y) &&;

        inline float getScale() const;
        inline float getRotation() const;
        inline Vector2f getTranslation() const;

        inline Vector2f operator*(Vector2f rhs) const;
        inline Transform operator*(Transform const& rhs) const;
        inline Transform& operator*=(Transform const& rhs);
        inline bool operator==(Transform const& rhs) const;
        inline bool operator!=(Transform const& rhs) const;

        inline Transform inverse() const;

        friend std::ostream& operator<<(std::ostream& stream,
                avg::Transform const& t)
        {
            return stream << "Transform{t{" << t.getTranslation()[0] << ", "
                << t.getTranslation()[1] << "}, s: " << t.getScale()
                << ", r: " << t.getRotation() << "}";
        }

    private:
        btl::ForceNoexcept<Vector2f> translation_;
        float scale_;
        float rotation_;
    };

    inline Transform::Transform(Vector2f translation,
                float scale, float rotation) :
        translation_(translation),
        scale_(scale),
        rotation_(rotation)
    {
    }

    inline Transform::~Transform()
    {
    }

    inline Transform&& Transform::scale(float scale) &&
    {
        *translation_ *= scale;
        scale_ *= scale;
        return std::move(*this);
    }

    inline Transform&& Transform::rotate(float angle) &&
    {
        return std::move(*this).setRotation(rotation_ + angle);
    }

    inline Transform&& Transform::translate(Vector2f v) &&
    {
        *translation_ += v;
        return std::move(*this);
    }

    inline Transform&& Transform::translate(float x, float y) &&
    {
        return std::move(*this).translate(Vector2f(x, y));
    }

    inline Transform&& Transform::setScale(float scale) &&
    {
        scale_ = scale;
        return std::move(*this);
    }

    inline Transform&& Transform::setRotation(float rotation) &&
    {
        float const pi = 3.1415927f;
        rotation_ = std::fmod(rotation + pi, 2.0f * pi) - pi;
        return std::move(*this);
    }

    inline Transform&& Transform::setTranslation(Vector2f v) &&
    {
        translation_ = v;
        return std::move(*this);
    }

    inline Transform&& Transform::setTranslation(float x, float y) &&
    {
        return std::move(*this).setTranslation(Vector2f(x, y));
    }

    inline float Transform::getScale() const
    {
        return scale_;
    }

    /**
     * @brief Returns angle (from -pi to pi) of rotation.
     */
    inline float Transform::getRotation() const
    {
        return rotation_;
    }

    inline Vector2f Transform::getTranslation() const
    {
        return *translation_;
    }

    inline Vector2f Transform::operator*(Vector2f rhs) const
    {
        float cosA = std::cos(rotation_);
        float sinA = std::sin(rotation_);

        Vector2f rotated(rhs[0] * cosA + rhs[1] * -sinA,
                rhs[0] * sinA + rhs[1] * cosA);
        return *translation_ + scale_ * rotated;
    }

    inline Transform Transform::operator*(Transform const& rhs) const
    {
        // o = t1 + s1 * R1 * (t2 + s2 * R2 * i)
        // o = t1 + s1 R1 t2 + s1 R1 s2 R2 * i
        // o = t1 + s1 R1 t2 + s1 s2 R1 R2 * i
        // t' = t1 + s1 R1 t2
        // s' = s1 * s2
        // r' = r1 + r2

        float cosA = std::cos(rhs.rotation_);
        float sinA = std::sin(rhs.rotation_);
        auto translation = Vector2f(
                rhs.translation_[0] * cosA + rhs.translation_[1] * -sinA,
                rhs.translation_[0] * sinA + rhs.translation_[1] * cosA);
        return Transform()
            .setTranslation(*translation_ + scale_ * translation)
            .setScale(scale_ * rhs.scale_)
            .setRotation(rotation_ + rhs.rotation_);
    }

    inline Transform& Transform::operator*=(Transform const& rhs)
    {
        *this = *this * rhs;
        return *this;
    }

    inline Transform Transform::inverse() const
    {
        // o = t + s R * i
        // R^-1 * (o - t) / s = i
        // R^-1 * -t * s^-1 + R^-1 * s^-1 * o
        // t' = r' s' -t
        // s' = s^-1
        // r' = r^-1
        Transform result;
        result.rotation_ = -rotation_;
        result.scale_ = 1.0f / scale_;
        float cosA = std::cos(result.rotation_);
        float sinA = std::sin(result.rotation_);
        *result.translation_ = result.scale_ *
            Vector2f(-translation_[0] * cosA + -translation_[1] * -sinA,
                    -translation_[0] * sinA + -translation_[1] * cosA);

        return result;
    }

    inline bool Transform::operator==(Transform const& rhs) const
    {
        const float sigma = 0.0001f;
        //const float pi = 3.1415927f;

        // Rotations are now expressed from -pi to pi.
        /*float rot1 = std::fmod(rotation_ + pi, 2.0f*pi) - pi;
        float rot2 = std::fmod(rhs.rotation_ + pi, 2.0f*pi) - pi;*/

        //std::cout << "rot1: " << rot1 << ", rot2: " << rot2 << std::endl;
        float rot1 = rotation_;
        float rot2 = rhs.rotation_;

        return std::abs(scale_ - rhs.scale_) < sigma
            && std::abs(rot1 - rot2) < sigma
            && std::abs(translation_[0] - rhs.translation_[0]) < sigma
            && std::abs(translation_[1] - rhs.translation_[1]) < sigma;
    }

    inline bool Transform::operator!=(Transform const& rhs) const
    {
        return !(*this == rhs);
    }
}

namespace btl
{
    template <> struct is_contiguously_hashable<avg::Transform>
        : std::true_type {};
}


