#pragma once

#include <btl/hash.h>

#include <Eigen/Geometry>

namespace ase
{
    typedef Eigen::Matrix4f Matrix4f;
    typedef Eigen::Matrix3f Matrix3f;
}

namespace btl
{
    template <> struct is_contiguously_hashable<ase::Matrix3f>
        : std::true_type {};
    template <> struct is_contiguously_hashable<ase::Matrix4f>
        : std::true_type {};
}

