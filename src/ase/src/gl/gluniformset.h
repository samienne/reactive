#pragma once

#include "uniformset.h"
#include "uniformsetimpl.h"
#include "uniformbufferrange.h"

#include <unordered_map>

namespace ase
{
    class GlRenderContext;

    class GlUniformSet : public UniformSetImpl
    {
    public:
        using ConstIterator = std::unordered_map<size_t,
              UniformBufferRange>::const_iterator;

        explicit GlUniformSet(GlRenderContext& context);

        void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                ) override;

        ConstIterator begin() const;
        ConstIterator end() const;

    private:
        std::unordered_map<size_t, UniformBufferRange> bindings_;

    };
} // namespace

