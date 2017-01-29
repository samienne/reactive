#pragma once

#include "vertexshaderimpl.h"
#include "glshader.h"

#include <GL/gl.h>

#include <string>

namespace ase
{
    class GlPlatform;
    class RenderContext;

    class GlVertexShader : public VertexShaderImpl
    {
    public:
        GlVertexShader(RenderContext& context, std::string const& source);
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

