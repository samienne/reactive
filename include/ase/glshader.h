#pragma once

#include "dispatcher.h"

#include <btl/visibility.h>

#include <GL/gl.h>

#include <string>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;

    class BTL_VISIBLE GlShader
    {
    public:
        GlShader(GlRenderContext& context, std::string const& source,
                GLenum shaderType);
        GlShader(GlShader const& other) = delete;
        GlShader(GlShader&& other) = delete;
        virtual ~GlShader();

        GlShader& operator=(GlShader const& other) = delete;
        GlShader& operator=(GlShader&& other) = delete;

    private:
        void create(Dispatched);
        void destroy();

    private:
        friend class GlProgram;
        GlRenderContext& context_;
        GLuint shader_;
    };
}

