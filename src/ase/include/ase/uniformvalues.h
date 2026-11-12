#pragma once

#include "shaderinterface.h"
#include "matrix.h"
#include "vector.h"

#include "asevisibility.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ase
{
    /**
     * @brief The UniformType a C++ value is written as.
     *
     * Matrices are Eigen column-major, integer vectors std::array.
     */
    template <class T>
    struct UniformTypeOf;

    template <> struct UniformTypeOf<float>
    { static constexpr UniformType value = UniformType::Float; };
    template <> struct UniformTypeOf<Vector2f>
    { static constexpr UniformType value = UniformType::Float2; };
    template <> struct UniformTypeOf<Vector3f>
    { static constexpr UniformType value = UniformType::Float3; };
    template <> struct UniformTypeOf<Vector4f>
    { static constexpr UniformType value = UniformType::Float4; };
    template <> struct UniformTypeOf<int32_t>
    { static constexpr UniformType value = UniformType::Int; };
    template <> struct UniformTypeOf<std::array<int32_t, 2>>
    { static constexpr UniformType value = UniformType::Int2; };
    template <> struct UniformTypeOf<std::array<int32_t, 3>>
    { static constexpr UniformType value = UniformType::Int3; };
    template <> struct UniformTypeOf<std::array<int32_t, 4>>
    { static constexpr UniformType value = UniformType::Int4; };
    template <> struct UniformTypeOf<uint32_t>
    { static constexpr UniformType value = UniformType::UInt; };
    template <> struct UniformTypeOf<std::array<uint32_t, 2>>
    { static constexpr UniformType value = UniformType::UInt2; };
    template <> struct UniformTypeOf<std::array<uint32_t, 3>>
    { static constexpr UniformType value = UniformType::UInt3; };
    template <> struct UniformTypeOf<std::array<uint32_t, 4>>
    { static constexpr UniformType value = UniformType::UInt4; };
    template <> struct UniformTypeOf<Matrix2f>
    { static constexpr UniformType value = UniformType::Mat2; };
    template <> struct UniformTypeOf<Matrix3f>
    { static constexpr UniformType value = UniformType::Mat3; };
    template <> struct UniformTypeOf<Matrix4f>
    { static constexpr UniformType value = UniformType::Mat4; };

    namespace detail
    {
        template <class T>
        std::enable_if_t<std::is_arithmetic_v<T>, void const*> uniformBytes(
                T const& value)
        {
            return &value;
        }

        template <class T>
        std::enable_if_t<!std::is_arithmetic_v<T>, void const*> uniformBytes(
                T const& value)
        {
            return value.data();
        }
    }

    /**
     * @brief The contents of a uniform block, set member by member.
     *
     *     ase::UniformValues params(iface, "BlurParams");
     *     params.set("radius", 12.0f);
     *     params.set("direction", ase::Vector2f(1.0f, 0.0f));
     *     params.set("weights", std::vector<float>{ 0.5f, 0.25f });
     *     ring.write(params);
     *
     * A value's type must match the declared member, an array member takes
     * a std::vector of exactly its length, and the name must be declared;
     * anything else throws std::invalid_argument. Members never set are
     * zero. data() and getSize() give the bytes to upload.
     */
    class ASE_EXPORT UniformValues
    {
    public:
        UniformValues(ShaderInterface const& iface, std::string_view blockName);
        explicit UniformValues(ShaderInterface::Block block);

        template <class T>
        void set(std::string_view name, T const& value)
        {
            writeElement(resolve(name, UniformTypeOf<T>::value, std::nullopt),
                    0, detail::uniformBytes(value));
        }

        template <class T>
        void set(std::string_view name, std::vector<T> const& values)
        {
            ShaderInterface::Member const& member = resolve(name,
                    UniformTypeOf<T>::value, values.size());

            for (size_t i = 0; i < values.size(); ++i)
                writeElement(member, i, detail::uniformBytes(values[i]));
        }

        ShaderInterface::Block const& getBlock() const;

        void const* data() const;
        size_t getSize() const;

    private:
        ShaderInterface::Member const& resolve(std::string_view name,
                UniformType type, std::optional<size_t> arrayCount) const;

        void writeElement(ShaderInterface::Member const& member, size_t index,
                void const* element);

    private:
        ShaderInterface::Block block_;
        std::vector<char> bytes_;
    };
} // namespace ase
