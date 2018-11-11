#pragma once

#include <btl/visibility.h>

#include <string>
#include <memory>

namespace ase
{
    class VertexShaderImpl;
    class RenderContext;
    class Platform;

    class BTL_VISIBLE VertexShader
    {
    public:
        VertexShader(RenderContext& context, std::string const& source);
        VertexShader(VertexShader const& other) = default;
        VertexShader(VertexShader&& other) = default;
        ~VertexShader();

        VertexShader& operator=(VertexShader const& other) = default;
        VertexShader& operator=(VertexShader&& other) = default;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        friend class Program;
        std::shared_ptr<VertexShaderImpl> deferred_;
        inline VertexShaderImpl* d() { return deferred_.get(); }
        inline VertexShaderImpl const* d() const { return deferred_.get(); }
    };
}

