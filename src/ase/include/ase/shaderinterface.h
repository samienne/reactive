#pragma once

#include "type.h"

#include "asevisibility.h"

#include <string>
#include <string_view>
#include <vector>

namespace ase
{
    /**
     * @brief The type of a uniform block member.
     *
     * Matrices are column-major, as GLSL lays them out.
     */
    enum class UniformType
    {
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4,
        UInt,
        UInt2,
        UInt3,
        UInt4,
        Mat2,
        Mat3,
        Mat4
    };

    /**
     * @brief The rule a uniform block's members are laid out by.
     */
    enum class BlockLayout
    {
        Std140
    };

    /**
     * @brief The kind of texture a sampler reads.
     */
    enum class SamplerType
    {
        Sampler2D
    };

    ASE_EXPORT std::string_view toString(UniformType type);

    class ShaderInterface;

    /**
     * @brief Lists every way a compiled program's interface differs from
     * the one declared for it; empty when they agree.
     *
     * A compiled block may be larger than declared, since a backend may pad
     * it; every other difference, including an input the declaration lacks,
     * is reported.
     */
    ASE_EXPORT std::vector<std::string> findMismatches(
            ShaderInterface const& declared,
            ShaderInterface const& compiled);

    /**
     * @brief Declares every input of a shader program.
     *
     * Built next to the shader sources and passed to
     * RenderContext::makeProgram. Each entry carries the integer slot the
     * backends bind by and the name the shader source uses for it:
     *
     *     ase::ShaderInterface iface;
     *     iface.attribute("pos", 0, ase::TypeFloat, 3)
     *         .block("MatrixBlock", 0, ase::BlockLayout::Std140, {
     *             { "worldViewProj", ase::UniformType::Mat4 },
     *         })
     *         .sampler("atlas", 0);
     *
     * Member offsets, array strides and the block size follow from the
     * layout rule; a caller never computes them. Adding an entry whose name
     * or slot is already taken throws std::invalid_argument.
     */
    class ASE_EXPORT ShaderInterface
    {
    public:
        struct ASE_EXPORT Attribute
        {
            std::string name;
            int location;
            Type type;
            int size;

            bool operator==(Attribute const& rhs) const;
            bool operator!=(Attribute const& rhs) const;
        };

        /**
         * @brief A block member as declared; arraySize 0 means not an array.
         */
        struct ASE_EXPORT MemberDecl
        {
            std::string name;
            UniformType type;
            int arraySize = 0;
        };

        /**
         * @brief A block member with its computed placement.
         *
         * arrayStride is 0 unless the member is an array.
         */
        struct ASE_EXPORT Member
        {
            std::string name;
            UniformType type;
            int arraySize;
            size_t offset;
            size_t arrayStride;

            bool operator==(Member const& rhs) const;
            bool operator!=(Member const& rhs) const;
        };

        struct ASE_EXPORT Block
        {
            std::string name;
            int binding;
            BlockLayout layout;
            std::vector<Member> members;
            size_t size;

            Member const* findMember(std::string_view name) const;

            bool operator==(Block const& rhs) const;
            bool operator!=(Block const& rhs) const;
        };

        struct ASE_EXPORT Sampler
        {
            std::string name;
            int unit;
            SamplerType type;

            bool operator==(Sampler const& rhs) const;
            bool operator!=(Sampler const& rhs) const;
        };

        ShaderInterface();
        ShaderInterface(ShaderInterface const&) = default;
        ShaderInterface(ShaderInterface&&) noexcept = default;
        ~ShaderInterface();

        ShaderInterface& operator=(ShaderInterface const&) = default;
        ShaderInterface& operator=(ShaderInterface&&) noexcept = default;

        /**
         * @brief Declares a vertex attribute of size components of type.
         */
        ShaderInterface& attribute(std::string name, int location, Type type,
                int size);

        /**
         * @brief Declares a uniform block and lays its members out.
         */
        ShaderInterface& block(std::string name, int binding,
                BlockLayout layout, std::vector<MemberDecl> members);

        /**
         * @brief Adds a block whose members are already placed, as a
         * backend's reflection reports them.
         */
        ShaderInterface& block(Block block);

        /**
         * @brief Declares a sampler bound to a texture unit.
         */
        ShaderInterface& sampler(std::string name, int unit,
                SamplerType type = SamplerType::Sampler2D);

        std::vector<Attribute> const& getAttributes() const;
        std::vector<Block> const& getBlocks() const;
        std::vector<Sampler> const& getSamplers() const;

        Attribute const* findAttribute(std::string_view name) const;
        Block const* findBlock(std::string_view name) const;
        Sampler const* findSampler(std::string_view name) const;

        bool operator==(ShaderInterface const& rhs) const;
        bool operator!=(ShaderInterface const& rhs) const;

    private:
        std::vector<Attribute> attributes_;
        std::vector<Block> blocks_;
        std::vector<Sampler> samplers_;
    };
}
