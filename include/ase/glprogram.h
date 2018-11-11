#pragma once

#include "programimpl.h"

#include <btl/visibility.h>

#include <GL/gl.h>

#include <string>
#include <map>

namespace ase
{
    class GlRenderContext;
    class GlVertexShader;
    class GlFragmentShader;
    class GlPlatform;
    struct GlFunctions;

    class BTL_VISIBLE GlProgram : public ProgramImpl
    {
    public:
        GlProgram(GlRenderContext& context, GlVertexShader const& vertexShader,
                GlFragmentShader const& fragmentShader);
        ~GlProgram();

        GlProgram(GlProgram const&) = delete;
        GlProgram(GlProgram&&) = delete;

        GlProgram& operator=(GlProgram const&) = delete;
        GlProgram& operator=(GlProgram&&) = delete;

        virtual int getUniformLocation(std::string const& name) const;
        virtual int getAttribLocation(std::string const& name) const;

    private:
        void destroy();
        void programIntrospection(GlFunctions const& gl);

    private:
        friend class GlRenderContext;
        GlRenderContext& context_;
        GLuint program_;
        std::map<std::string, int> uniformLocations_;
        std::map<std::string, int> attribLocations_;
    };
}

