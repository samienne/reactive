#pragma once

#include "programimpl.h"
#include "shaderinterface.h"

#include "asevisibility.h"

#include "systemgl.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace ase
{
    class GlRenderContext;
    class GlShader;
    class GlPlatform;
    struct GlFunctions;

    class ASE_EXPORT GlProgram : public ProgramImpl
    {
    public:
        GlProgram(GlRenderContext& context, GlShader const& vertexShader,
                GlShader const& fragmentShader, ShaderInterface iface);
        ~GlProgram();

        GlProgram(GlProgram const&) = delete;
        GlProgram(GlProgram&&) = delete;

        GlProgram& operator=(GlProgram const&) = delete;
        GlProgram& operator=(GlProgram&&) = delete;

        GLuint getGlObject() const;

        int getUniformLocation(std::string const& name) const override;
        int getAttribLocation(std::string const& name) const override;
        ShaderInterface const& getInterface() const override;
        std::optional<ShaderInterface> reflect() const override;

    private:
        void destroy();
        void bindInterface(GlFunctions const& gl);
        void programIntrospection(GlFunctions const& gl);

    private:
        friend class GlRenderContext;
        GlRenderContext& context_;
        GLuint program_;
        ShaderInterface interface_;
        std::unordered_map<std::string, int> uniformLocations_;
        std::unordered_map<std::string, int> attribLocations_;
    };
}
