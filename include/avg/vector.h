#include <btl/hash.h>

#include <ase/vector.h>

#include <ostream>

namespace avg
{
    using Vector2i = ase::Vector2i;
    using Vector3i = ase::Vector3i;
    using Vector4i = ase::Vector4i;

    using Vector2f = ase::Vector2f;
    using Vector3f = ase::Vector3f;
    using Vector4f = ase::Vector4f;

    using Matrix2f = Eigen::Matrix2f;
}

/*
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
*/


