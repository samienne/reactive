#pragma once

#include "shaderimpl.h"
#include "dispatcher.h"

#include "asevisibility.h"

#include "systemgl.h"

namespace ase
{
    class Buffer;
    class GlRenderContext;

    class ASE_EXPORT GlShader : public ShaderImpl
    {
    public:
        GlShader(GlRenderContext& context, ShaderStage stage,
                Buffer const& source);
        GlShader(GlShader const& other) = delete;
        GlShader(GlShader&& other) = delete;
        ~GlShader() override;

        GlShader& operator=(GlShader const& other) = delete;
        GlShader& operator=(GlShader&& other) = delete;

        ShaderStage getStage() const override;

    private:
        void destroy();

    private:
        friend class GlProgram;
        GlRenderContext& context_;
        ShaderStage stage_;
        GLuint shader_ = 0;
    };
}
