#pragma once

#include "shaderinterface.h"

#include "asevisibility.h"

#include <string>
#include <string_view>
#include <vector>

namespace ase
{
    /**
     * @brief Where a C++ struct keeps one member of a uniform block.
     *
     * Made by blockMember; UniformBlock checks it against the declaration.
     */
    template <class T>
    struct UniformBlockMember
    {
        std::string_view name;
        size_t size;
        size_t (*offsetIn)(T const&);
    };

    namespace detail
    {
        template <class P>
        struct MemberPointer;

        template <class T, class M>
        struct MemberPointer<M T::*>
        {
            using Class = T;
            using Member = M;
        };
    }

    /**
     * @brief Names the struct member that holds a block member:
     * blockMember<&MatrixBlock::worldViewProj>("worldViewProj").
     */
    template <auto M>
    UniformBlockMember<typename detail::MemberPointer<decltype(M)>::Class>
    blockMember(std::string_view name)
    {
        using Pointer = detail::MemberPointer<decltype(M)>;
        using T = typename Pointer::Class;

        return { name, sizeof(typename Pointer::Member),
            [](T const& object) -> size_t
            {
                return static_cast<size_t>(
                        reinterpret_cast<char const*>(&(object.*M))
                        - reinterpret_cast<char const*>(&object));
            } };
    }

    /**
     * @brief The block a declaration names.
     *
     * Throws std::invalid_argument when the interface has no such block.
     */
    ASE_EXPORT ShaderInterface::Block const& findDeclaredBlock(
            ShaderInterface const& iface, std::string_view blockName);

    /**
     * @brief A struct member's placement, as UniformBlock checks it.
     */
    struct UniformMemberLayout
    {
        std::string name;
        size_t offset;
        size_t size;
    };

    /**
     * @brief Throws std::invalid_argument unless a struct of structSize
     * whose members lie as given matches the block member for member.
     *
     * Every declared member must be listed exactly once.
     */
    ASE_EXPORT void checkUniformBlockLayout(ShaderInterface::Block const& block,
            size_t structSize, std::vector<UniformMemberLayout> const& members);

    /**
     * @brief A uniform block held as a C++ struct of the same layout.
     *
     * Construction checks the struct's size and each listed member's offset
     * and size against the declaration, so a struct that mis-pads a std140
     * block is rejected:
     *
     *     struct MatrixBlock { ase::Matrix4f worldViewProj; };
     *
     *     ase::UniformBlock<MatrixBlock> matrices(iface, "MatrixBlock", {
     *         ase::blockMember<&MatrixBlock::worldViewProj>("worldViewProj"),
     *     });
     *
     *     matrices->worldViewProj = projection;
     *     ring.write(matrices);
     *
     * data() and getSize() give the bytes to upload.
     */
    template <class T>
    class UniformBlock
    {
    public:
        UniformBlock(ShaderInterface const& iface, std::string_view blockName,
                std::vector<UniformBlockMember<T>> const& members) :
            UniformBlock(findDeclaredBlock(iface, blockName), members)
        {
        }

        UniformBlock(ShaderInterface::Block block,
                std::vector<UniformBlockMember<T>> const& members) :
            block_(std::move(block)),
            value_()
        {
            std::vector<UniformMemberLayout> layouts;
            layouts.reserve(members.size());
            for (auto const& member : members)
            {
                layouts.push_back({ std::string(member.name),
                        member.offsetIn(value_), member.size });
            }

            checkUniformBlockLayout(block_, sizeof(T), layouts);
        }

        T& operator*()
        {
            return value_;
        }

        T const& operator*() const
        {
            return value_;
        }

        T* operator->()
        {
            return &value_;
        }

        T const* operator->() const
        {
            return &value_;
        }

        ShaderInterface::Block const& getBlock() const
        {
            return block_;
        }

        void const* data() const
        {
            return &value_;
        }

        size_t getSize() const
        {
            return sizeof(T);
        }

    private:
        ShaderInterface::Block block_;
        T value_;
    };
} // namespace ase
