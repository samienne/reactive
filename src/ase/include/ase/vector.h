#pragma once

#include <btl/hash.h>

#include <Eigen/Geometry>

#include <ostream>

namespace ase
{
    using Vector2i = Eigen::Matrix<int64_t, 2, 1>;//Eigen::Vector2i;
    using Vector3i = Eigen::Matrix<int64_t, 3, 1>;//Eigen::Vector3i;
    using Vector4i = Eigen::Matrix<int64_t, 4, 1>;//Eigen::Vector4i;

    using Vector2f = Eigen::Vector2f;
    using Vector3f = Eigen::Vector3f;
    using Vector4f = Eigen::Vector4f;
}

inline std::ostream& operator<<(std::ostream& stream, ase::Vector2f const& v)
{
    return stream << "v(" << v[0] << ", " << v[1] << ")";
}

namespace btl
{
    template <> struct is_contiguously_hashable<ase::Vector2f>
        : std::true_type {};
    template <> struct is_contiguously_hashable<ase::Vector3f>
        : std::true_type {};
    template <> struct is_contiguously_hashable<ase::Vector4f>
        : std::true_type {};
    template <> struct is_contiguously_hashable<ase::Vector2i>
        : std::true_type {};
    template <> struct is_contiguously_hashable<ase::Vector3i>
        : std::true_type {};
    template <> struct is_contiguously_hashable<ase::Vector4i>
        : std::true_type {};
}

