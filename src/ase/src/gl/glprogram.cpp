#include "glprogram.h"

#include "glshader.h"
#include "glrendercontext.h"
#include "glplatform.h"
#include "gltype.h"
#include "glfunctions.h"

#include "rendercontext.h"

#include "debug.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace ase
{

namespace
{
    constexpr GLsizei maxNameLength = 256;

    struct AttributeType
    {
        Type type;
        int size;
    };

    std::optional<AttributeType> attributeTypeFromGl(GLenum type)
    {
        switch (type)
        {
            case GL_FLOAT: return AttributeType{ TypeFloat, 1 };
            case GL_FLOAT_VEC2: return AttributeType{ TypeFloat, 2 };
            case GL_FLOAT_VEC3: return AttributeType{ TypeFloat, 3 };
            case GL_FLOAT_VEC4: return AttributeType{ TypeFloat, 4 };
            case GL_INT: return AttributeType{ TypeInt, 1 };
            case GL_INT_VEC2: return AttributeType{ TypeInt, 2 };
            case GL_INT_VEC3: return AttributeType{ TypeInt, 3 };
            case GL_INT_VEC4: return AttributeType{ TypeInt, 4 };
            case GL_UNSIGNED_INT: return AttributeType{ TypeUInt, 1 };
            case GL_UNSIGNED_INT_VEC2: return AttributeType{ TypeUInt, 2 };
            case GL_UNSIGNED_INT_VEC3: return AttributeType{ TypeUInt, 3 };
            case GL_UNSIGNED_INT_VEC4: return AttributeType{ TypeUInt, 4 };
        }

        return std::nullopt;
    }

    std::optional<UniformType> uniformTypeFromGl(GLenum type)
    {
        switch (type)
        {
            case GL_FLOAT: return UniformType::Float;
            case GL_FLOAT_VEC2: return UniformType::Float2;
            case GL_FLOAT_VEC3: return UniformType::Float3;
            case GL_FLOAT_VEC4: return UniformType::Float4;
            case GL_INT: return UniformType::Int;
            case GL_INT_VEC2: return UniformType::Int2;
            case GL_INT_VEC3: return UniformType::Int3;
            case GL_INT_VEC4: return UniformType::Int4;
            case GL_UNSIGNED_INT: return UniformType::UInt;
            case GL_UNSIGNED_INT_VEC2: return UniformType::UInt2;
            case GL_UNSIGNED_INT_VEC3: return UniformType::UInt3;
            case GL_UNSIGNED_INT_VEC4: return UniformType::UInt4;
            case GL_FLOAT_MAT2: return UniformType::Mat2;
            case GL_FLOAT_MAT3: return UniformType::Mat3;
            case GL_FLOAT_MAT4: return UniformType::Mat4;
        }

        return std::nullopt;
    }

    bool startsWith(std::string_view str, std::string_view prefix)
    {
        return str.substr(0, prefix.size()) == prefix;
    }

    bool endsWith(std::string_view str, std::string_view suffix)
    {
        return str.size() >= suffix.size()
            && str.substr(str.size() - suffix.size()) == suffix;
    }

    GLint uniformParameter(GlFunctions const& gl, GLuint program,
            GLuint index, GLenum pname)
    {
        GLint value = 0;
        gl.glGetActiveUniformsiv(program, 1, &index, pname, &value);
        return value;
    }

    GLint blockParameter(GlFunctions const& gl, GLuint program,
            GLuint index, GLenum pname)
    {
        GLint value = 0;
        gl.glGetActiveUniformBlockiv(program, index, pname, &value);
        return value;
    }

    std::vector<ShaderInterface::Attribute> reflectAttributes(
            GlFunctions const& gl, GLuint program)
    {
        std::vector<ShaderInterface::Attribute> attributes;

        GLint count = 0;
        gl.glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);

        for (GLint i = 0; i < count; ++i)
        {
            char name[maxNameLength];
            GLsizei len = 0;
            GLint size = 0;
            GLenum type = 0;
            gl.glGetActiveAttrib(program, static_cast<GLuint>(i),
                    maxNameLength, &len, &size, &type, name);

            if (startsWith(name, "gl_"))
                continue;

            auto attributeType = attributeTypeFromGl(type);
            if (!attributeType)
            {
                throw std::runtime_error("Attribute '" + std::string(name)
                        + "' has a type the shader interface cannot"
                        " describe");
            }

            attributes.push_back(ShaderInterface::Attribute{ name,
                    gl.glGetAttribLocation(program, name),
                    attributeType->type, attributeType->size });
        }

        return attributes;
    }

    // GL names an instanced block's members "Block.member" and the first
    // element of an array "member[0]"; the interface uses the bare name.
    std::string memberName(std::string_view glName, std::string_view block,
            bool& isArray)
    {
        if (startsWith(glName, block) && glName.size() > block.size()
                && glName[block.size()] == '.')
        {
            glName.remove_prefix(block.size() + 1);
        }

        isArray = endsWith(glName, "[0]");
        if (isArray)
            glName.remove_suffix(3);

        return std::string(glName);
    }

    ShaderInterface::Block reflectBlock(GlFunctions const& gl,
            GLuint program, GLuint blockIndex)
    {
        char name[maxNameLength];
        GLsizei len = 0;
        gl.glGetActiveUniformBlockName(program, blockIndex, maxNameLength,
                &len, name);

        ShaderInterface::Block block;
        block.name = name;
        block.binding = blockParameter(gl, program, blockIndex,
                GL_UNIFORM_BLOCK_BINDING);
        block.layout = BlockLayout::Std140;
        block.size = static_cast<size_t>(blockParameter(gl, program,
                    blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE));

        GLint memberCount = blockParameter(gl, program, blockIndex,
                GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS);
        std::vector<GLint> indices(static_cast<size_t>(memberCount));
        if (memberCount > 0)
        {
            gl.glGetActiveUniformBlockiv(program, blockIndex,
                    GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, indices.data());
        }

        for (GLint index : indices)
        {
            GLuint const uniform = static_cast<GLuint>(index);
            char uniformName[maxNameLength];
            GLint size = 0;
            GLenum type = 0;
            gl.glGetActiveUniform(program, uniform, maxNameLength, &len,
                    &size, &type, uniformName);

            auto uniformType = uniformTypeFromGl(type);
            if (!uniformType)
            {
                throw std::runtime_error("Member '" + block.name + "."
                        + uniformName + "' has a type the shader interface"
                        " cannot describe");
            }

            if (uniformParameter(gl, program, uniform,
                        GL_UNIFORM_IS_ROW_MAJOR))
            {
                throw std::runtime_error("Member '" + block.name + "."
                        + uniformName + "' is row-major");
            }

            bool isArray = false;
            ShaderInterface::Member member;
            member.name = memberName(uniformName, block.name, isArray);
            member.type = *uniformType;
            member.arraySize = isArray ? size : 0;
            member.offset = static_cast<size_t>(uniformParameter(gl,
                        program, uniform, GL_UNIFORM_OFFSET));
            member.arrayStride = isArray
                ? static_cast<size_t>(uniformParameter(gl, program, uniform,
                            GL_UNIFORM_ARRAY_STRIDE))
                : 0;

            block.members.push_back(std::move(member));
        }

        std::sort(block.members.begin(), block.members.end(),
                [](auto const& a, auto const& b)
                {
                    return a.offset < b.offset;
                });

        return block;
    }

    std::vector<ShaderInterface::Sampler> reflectSamplers(
            GlFunctions const& gl, GLuint program)
    {
        std::vector<ShaderInterface::Sampler> samplers;

        GLint count = 0;
        gl.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

        for (GLint i = 0; i < count; ++i)
        {
            GLuint const uniform = static_cast<GLuint>(i);
            char name[maxNameLength];
            GLsizei len = 0;
            GLint size = 0;
            GLenum type = 0;
            gl.glGetActiveUniform(program, uniform, maxNameLength, &len,
                    &size, &type, name);

            if (type != GL_SAMPLER_2D || uniformParameter(gl, program,
                        uniform, GL_UNIFORM_BLOCK_INDEX) != -1)
            {
                continue;
            }

            GLint unit = 0;
            gl.glGetUniformiv(program, gl.glGetUniformLocation(program, name),
                    &unit);

            samplers.push_back(ShaderInterface::Sampler{ name, unit,
                    SamplerType::Sampler2D });
        }

        return samplers;
    }

    // An undeclared block or sampler keeps the GL default binding or unit 0,
    // which can collide with a declared one, so given a declaration those go
    // to undeclared instead of the result.
    ShaderInterface reflectProgram(GlFunctions const& gl, GLuint program,
            ShaderInterface const* declared = nullptr,
            std::vector<std::string>* undeclared = nullptr)
    {
        ShaderInterface result;

        for (auto const& attribute : reflectAttributes(gl, program))
        {
            result.attribute(attribute.name, attribute.location,
                    attribute.type, attribute.size);
        }

        GLint blockCount = 0;
        gl.glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &blockCount);
        for (GLint i = 0; i < blockCount; ++i)
        {
            auto block = reflectBlock(gl, program, static_cast<GLuint>(i));
            if (!declared || declared->findBlock(block.name))
                result.block(std::move(block));
            else
            {
                undeclared->push_back("block '" + block.name
                        + "' is compiled but not declared");
            }
        }

        for (auto const& sampler : reflectSamplers(gl, program))
        {
            if (!declared || declared->findSampler(sampler.name))
                result.sampler(sampler.name, sampler.unit, sampler.type);
            else
            {
                undeclared->push_back("sampler '" + sampler.name
                        + "' is compiled but not declared");
            }
        }

        return result;
    }

    std::string readInfoLog(GlFunctions const& gl, GLuint program)
    {
        GLsizei len = 0;
        gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        std::string log(static_cast<size_t>(len), '\0');
        gl.glGetProgramInfoLog(program, len, &len, log.data());
        log.resize(static_cast<size_t>(len));

        return log;
    }

    std::string join(std::vector<std::string> const& lines)
    {
        std::string result;
        for (auto const& line : lines)
            result += "  " + line + "\n";

        return result;
    }
} // anonymous namespace

GlProgram::GlProgram(GlRenderContext& context, GlShader const& vertexShader,
        GlShader const& fragmentShader, ShaderInterface iface) :
    context_(context),
    program_(0),
    interface_(std::move(iface))
{
    try
    {
        bool failed = false;
        std::string logStr;
        std::vector<std::string> mismatches;

        context_.dispatchBg([&, this](GlFunctions const& gl)
            {
                program_ = gl.glCreateProgram();
                gl.glAttachShader(program_, vertexShader.shader_);
                gl.glAttachShader(program_, fragmentShader.shader_);

                for (auto const& attribute : interface_.getAttributes())
                {
                    gl.glBindAttribLocation(program_,
                            static_cast<GLuint>(attribute.location),
                            attribute.name.c_str());
                }

                gl.glLinkProgram(program_);

                GLint status;
                gl.glGetProgramiv(program_, GL_LINK_STATUS, &status);
                if (status == GL_FALSE)
                {
                    failed = true;
                    logStr = readInfoLog(gl, program_);
                    glFlush();
                    return;
                }

                bindInterface(gl);
                programIntrospection(gl);

#ifndef NDEBUG
                try
                {
                    ShaderInterface compiled = reflectProgram(gl, program_,
                            &interface_, &mismatches);
                    for (auto& mismatch : findMismatches(interface_, compiled))
                        mismatches.push_back(std::move(mismatch));
                }
                catch (std::exception const& e)
                {
                    mismatches.push_back(e.what());
                }
#endif

                // The main context sees the program's state only once this
                // one has flushed it, bindings and sampler units included.
                glFlush();
            });

        context_.waitBg();

        if (failed)
        {
            DBG("Failed to link program: %1", logStr);
            throw std::runtime_error("Unable to link shader program:\n"
                    + logStr);
        }

        if (!mismatches.empty())
        {
            throw std::runtime_error("Shader program disagrees with its"
                    " declared interface:\n" + join(mismatches));
        }
    }
    catch (...)
    {
        destroy();
        throw;
    }
}

GlProgram::~GlProgram()
{
    destroy();
}

GLuint GlProgram::getGlObject() const
{
    return program_;
}

int GlProgram::getUniformLocation(std::string const& name) const
{
    auto i = uniformLocations_.find(name);

    if (i == uniformLocations_.end())
        return -1;

    return i->second;
}

int GlProgram::getAttribLocation(std::string const& name) const
{
    auto i = attribLocations_.find(name);

    if (i == attribLocations_.end())
        return -1;

    return i->second;
}

ShaderInterface const& GlProgram::getInterface() const
{
    return interface_;
}

std::optional<ShaderInterface> GlProgram::reflect() const
{
    std::optional<ShaderInterface> result;
    std::string error;
    GLuint const program = program_;

    context_.dispatchBg([&](GlFunctions const& gl)
        {
            try
            {
                result = reflectProgram(gl, program);
            }
            catch (std::exception const& e)
            {
                error = e.what();
            }
        });

    context_.waitBg();

    if (!error.empty())
        throw std::runtime_error(error);

    return result;
}

void GlProgram::destroy()
{
    if (program_)
    {
        GLuint program = program_;

        context_.dispatchBg([program](GlFunctions const& gl)
            {
                gl.glDeleteProgram(program);
            });
    }
}

void GlProgram::bindInterface(GlFunctions const& gl)
{
    for (auto const& block : interface_.getBlocks())
    {
        GLuint index = gl.glGetUniformBlockIndex(program_,
                block.name.c_str());
        if (index != GL_INVALID_INDEX)
        {
            gl.glUniformBlockBinding(program_, index,
                    static_cast<GLuint>(block.binding));
        }
    }

    if (interface_.getSamplers().empty())
        return;

    GLint previous = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
    gl.glUseProgram(program_);

    for (auto const& sampler : interface_.getSamplers())
    {
        GLint location = gl.glGetUniformLocation(program_,
                sampler.name.c_str());
        if (location != -1)
        {
            GLint const unit = sampler.unit;
            gl.glUniform1iv(location, 1, &unit);
        }
    }

    gl.glUseProgram(static_cast<GLuint>(previous));
}

void GlProgram::programIntrospection(GlFunctions const& gl)
{
    GLint count;
    gl.glGetProgramiv(program_, GL_ACTIVE_ATTRIBUTES, &count);

    char name[maxNameLength];
    GLsizei len;
    GLint size;
    GLenum type;

    for (int i = 0; i < count; ++i)
    {
        gl.glGetActiveAttrib(program_, i, maxNameLength, &len, &size, &type,
                name);
        attribLocations_[name] = gl.glGetAttribLocation(program_, name);
    }

    gl.glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);

    for (int i = 0; i < count; ++i)
    {
        gl.glGetActiveUniform(program_, i, maxNameLength, &len, &size, &type,
                name);
        uniformLocations_[name] = gl.glGetUniformLocation(program_, name);
    }
}

} // namespace ase
