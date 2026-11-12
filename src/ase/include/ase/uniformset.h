#pragma once

#include "asevisibility.h"

#include <memory>
#include <string_view>

namespace ase
{
    class UniformSetImpl;
    class ShaderInterface;
    struct UniformBufferRange;

    /**
     * @brief The uniform buffer ranges a draw binds to a program's blocks.
     *
     * Made by RenderContext::makeUniformSet for a declared interface:
     *
     *     ase::UniformSet set = context.makeUniformSet(iface);
     *     set.bindUniformBufferRange("MatrixBlock", range);
     *
     * Binding a block again replaces its range. Naming a block or binding
     * the interface does not declare, or a range smaller than the block,
     * throws std::invalid_argument.
     */
    class ASE_EXPORT UniformSet
    {
    public:
        explicit UniformSet(std::shared_ptr<UniformSetImpl> impl);

        bool operator==(UniformSet const& rhs) const;
        bool operator!=(UniformSet const& rhs) const;
        bool operator<(UniformSet const& rhs) const;
        bool operator>(UniformSet const& rhs) const;

        ShaderInterface const& getInterface() const;

        void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                );

        void bindUniformBufferRange(
                std::string_view blockName,
                UniformBufferRange const& buffer
                );

        /**
         * @brief Casts the internal implementation to requested type.
         *
         * This is for internal use only and expects that you know the
         * actual type of implementation.
         */
        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    protected:
        inline UniformSetImpl* d() { return deferred_.get(); }
        inline UniformSetImpl const* d() const { return deferred_.get(); }

    private:
        std::shared_ptr<UniformSetImpl> deferred_;
    };
} // namespace ase

