#pragma once

#include "uniformsetimpl.h"
#include "uniformbufferrange.h"

#include "asevisibility.h"

#include <map>

namespace ase
{
    /**
     * @brief A uniform set that only records its bindings, for tests to
     * read back.
     */
    class ASE_EXPORT DummyUniformSet : public UniformSetImpl
    {
    public:
        explicit DummyUniformSet(ShaderInterface iface);

        void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                ) override;

        /**
         * @brief The range bound at each binding, latest binding winning.
         */
        std::map<int, UniformBufferRange> const& getBindings() const;

    private:
        std::map<int, UniformBufferRange> bindings_;
    };
} // namespace ase
