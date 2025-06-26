#pragma once

#include "vertexshaderimpl.h"
#include "glshader.h"

#include "asevisibility.h"

#include "systemgl.h"

#include <string>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;

    class ASE_EXPORT GlVertexShader : public VertexShaderImpl
    {
    public:
        GlVertexShader(GlRenderContext& context, std::string const& source);
        GlVertexShader(GlVertexShader const& other) = delete;
        GlVertexShader(GlVertexShader&& other) = delete;
        virtual ~GlVertexShader();

        GlVertexShader& operator=(GlVertexShader const& other) = delete;
        GlVertexShader& operator=(GlVertexShader&& other) = delete;

    private:
        friend class GlProgram;
        GlShader shader_;
    };
}

