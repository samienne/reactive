#pragma once

#include "vertexshaderimpl.h"
#include "glshader.h"

#include <btl/visibility.h>

#include <GL/gl.h>

#include <string>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;

    class BTL_VISIBLE GlVertexShader : public VertexShaderImpl
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

