#pragma once

#include "programimpl.h"

#include <btl/visibility.h>

#include <GL/gl.h>

#include <string>
#include <map>

namespace ase
{
    class RenderContext;
    class GlVertexShader;
    class GlFragmentShader;
    class GlPlatform;
    struct GlFunctions;

    class BTL_VISIBLE GlProgram : public ProgramImpl
    {
    public:
        GlProgram(RenderContext& context, GlVertexShader const& vertexShader,
                GlFragmentShader const& fragmentShader);
        ~GlProgram();

        virtual int getUniformLocation(std::string const& name) const;
        virtual int getAttribLocation(std::string const& name) const;

    private:
        void destroy();
        void programIntrospection(GlFunctions const& gl);

    private:
        friend class GlRenderContext;
        GlPlatform* platform_;
        GLuint program_;
        std::map<std::string, int> uniformLocations_;
        std::map<std::string, int> attribLocations_;
    };
}

