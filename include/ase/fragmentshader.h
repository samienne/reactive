#pragma once

#include <btl/visibility.h>

#include <string>
#include <memory>

namespace ase
{
    class FragmentShaderImpl;
    class RenderContext;
    class Platform;

    class BTL_VISIBLE FragmentShader
    {
    public:
        FragmentShader(RenderContext& context, std::string const& source);
        FragmentShader(FragmentShader const& other) = default;
        FragmentShader(FragmentShader&& other) = default;
        ~FragmentShader();

        FragmentShader& operator=(FragmentShader const& other) = default;
        FragmentShader& operator=(FragmentShader&& other) = default;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        friend class Program;
        std::shared_ptr<FragmentShaderImpl> deferred_;
        inline FragmentShaderImpl* d() { return deferred_.get(); }
        inline FragmentShaderImpl const* d() const { return deferred_.get(); }
    };
}

