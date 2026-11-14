#pragma once

#include "shaderstage.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class ShaderImpl;

    /**
     * @brief A compiled shader for one stage.
     *
     * Made by RenderContext::makeShader and combined into a Program by
     * RenderContext::makeProgram. It carries no interface of its own; the
     * program's ShaderInterface describes the inputs of all its stages.
     */
    class ASE_EXPORT Shader
    {
    public:
        Shader(std::shared_ptr<ShaderImpl> impl);
        Shader(Shader const& other) = default;
        Shader(Shader&& other) noexcept = default;
        ~Shader();

        Shader& operator=(Shader const& other) = default;
        Shader& operator=(Shader&& other) noexcept = default;

        ShaderStage getStage() const;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        std::shared_ptr<ShaderImpl> deferred_;
        inline ShaderImpl* d() { return deferred_.get(); }
        inline ShaderImpl const* d() const { return deferred_.get(); }
    };
}
