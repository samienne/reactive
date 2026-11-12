#pragma once

#include "uniformset.h"
#include "uniformsetimpl.h"
#include "uniformbufferrange.h"

#include <cstdint>
#include <unordered_map>

namespace ase
{
    class GlRenderContext;

    class GlUniformSet : public UniformSetImpl
    {
    public:
        using ConstIterator = std::unordered_map<size_t,
              UniformBufferRange>::const_iterator;

        GlUniformSet(GlRenderContext& context, ShaderInterface iface);

        void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                ) override;

        ConstIterator begin() const;
        ConstIterator end() const;

        /**
         * @brief Changes on every bind.
         */
        uint64_t getVersion() const;

    private:
        std::unordered_map<size_t, UniformBufferRange> bindings_;
        uint64_t version_ = 0;

    };
} // namespace

