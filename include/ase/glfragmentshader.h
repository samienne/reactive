#pragma once

#include "fragmentshaderimpl.h"

#include "glshader.h"

#include <string>

namespace ase
{
    class RenderContext;

    class GlFragmentShader : public FragmentShaderImpl
    {
    public:
        GlFragmentShader(RenderContext& context, std::string const& source);
        GlFragmentShader(GlFragmentShader const& other) = delete;
        GlFragmentShader(GlFragmentShader&& other) = delete;
        virtual ~GlFragmentShader();

        GlFragmentShader& operator=(GlFragmentShader const& other) = delete;
        GlFragmentShader& operator=(GlFragmentShader&& other) = delete;

    private:
        friend class GlProgram;
        GlShader shader_;
    };
}

