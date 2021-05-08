#include "gldispatchedcontext.h"

#include <cassert>

namespace ase
{

GlDispatchedContext::GlDispatchedContext()
{
}

void GlDispatchedContext::wait()
{
    dispatcher_.wait();
}

void GlDispatchedContext::unsetIdleFunc(Dispatched d)
{
    dispatcher_.unsetIdleFunc(d);
}

bool GlDispatchedContext::hasIdleFunc(Dispatched d)
{
    return dispatcher_.hasIdleFunc(d);
}

void GlDispatchedContext::setGlFunctions(GlFunctions gl)
{
    assert(gl.glVertexAttribPointer);
    assert(gl.glDisableVertexAttribArray);
    assert(gl.glEnableVertexAttribArray);
    assert(gl.glUniform1fv);
    assert(gl.glUniform2fv);
    assert(gl.glUniform3fv);
    assert(gl.glUniform4fv);
    assert(gl.glUniform1iv);
    assert(gl.glUniform2iv);
    assert(gl.glUniform3iv);
    assert(gl.glUniform4iv);
    assert(gl.glUniform1uiv);
    assert(gl.glUniform2uiv);
    assert(gl.glUniform3uiv);
    assert(gl.glUniform4uiv);
    assert(gl.glUniformMatrix4fv);
    assert(gl.glUseProgram);
    assert(gl.glBindBuffer);
    assert(gl.glGenBuffers);
    assert(gl.glBufferData);
    assert(gl.glDeleteBuffers);
    assert(gl.glCreateProgram);
    assert(gl.glAttachShader);
    assert(gl.glLinkProgram);
    assert(gl.glGetProgramiv);
    assert(gl.glGetProgramInfoLog);
    assert(gl.glGetActiveAttrib);
    assert(gl.glGetAttribLocation);
    assert(gl.glGetActiveUniform);
    assert(gl.glGetUniformLocation);
    assert(gl.glDeleteProgram);
    assert(gl.glCreateShader);
    assert(gl.glShaderSource);
    assert(gl.glCompileShader);
    assert(gl.glGetShaderiv);
    assert(gl.glGetShaderInfoLog);
    assert(gl.glDeleteShader);
    assert(gl.glGenFramebuffers);
    assert(gl.glDeleteFramebuffers);
    assert(gl.glFramebufferTexture2D);
    assert(gl.glBindFramebuffer);
    assert(gl.glGenVertexArrays);
    assert(gl.glBindVertexArray);
    assert(gl.glDeleteVertexArrays);
    assert(gl.glBindBufferRange);
    assert(gl.glGetUniformBlockIndex);
    assert(gl.glActiveTexture);
    assert(gl.glGenRenderbuffers);
    assert(gl.glRenderbufferStorage);
    assert(gl.glDeleteRenderbuffers);
    assert(gl.glBindRenderbuffer);
    assert(gl.glFenceSync);
    assert(gl.glClientWaitSync);
    assert(gl.glDeleteSync);

    gl_ = std::move(gl);
}

GlFunctions const& GlDispatchedContext::getGlFunctions() const
{
    return gl_;
}

} // namespace ase

