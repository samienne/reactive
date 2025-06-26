#pragma once

#include "asevisibility.h"

#include "systemgl.h"

namespace ase
{
    struct ASE_EXPORT GlFunctions
    {
        PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
        PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
        PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
        PFNGLUNIFORM1FVPROC glUniform1fv = nullptr;
        PFNGLUNIFORM2FVPROC glUniform2fv = nullptr;
        PFNGLUNIFORM3FVPROC glUniform3fv = nullptr;
        PFNGLUNIFORM4FVPROC glUniform4fv = nullptr;
        PFNGLUNIFORM1IVPROC glUniform1iv = nullptr;
        PFNGLUNIFORM2IVPROC glUniform2iv = nullptr;
        PFNGLUNIFORM3IVPROC glUniform3iv = nullptr;
        PFNGLUNIFORM4IVPROC glUniform4iv = nullptr;
        PFNGLUNIFORM1UIVPROC glUniform1uiv = nullptr;
        PFNGLUNIFORM2UIVPROC glUniform2uiv = nullptr;
        PFNGLUNIFORM3UIVPROC glUniform3uiv = nullptr;
        PFNGLUNIFORM4UIVPROC glUniform4uiv = nullptr;
        PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
        PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
        PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
        PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
        PFNGLBUFFERDATAPROC glBufferData = nullptr;
        PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
        PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
        PFNGLATTACHSHADERPROC glAttachShader = nullptr;
        PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
        PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
        PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
        PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib = nullptr;
        PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = nullptr;
        PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform = nullptr;
        PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
        PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
        PFNGLCREATESHADERPROC glCreateShader = nullptr;
        PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
        PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
        PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
        PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
        PFNGLDELETESHADERPROC glDeleteShader = nullptr;
        PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
        PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = nullptr;
        PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
        PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
        PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
        PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
        PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
        PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
        PFNGLBINDBUFFERRANGEPROC glBindBufferRange = nullptr;
        PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex = nullptr;
        PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
        PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = nullptr;
        PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = nullptr;
        PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = nullptr;
        PFNGLFENCESYNCPROC glFenceSync = nullptr;
        PFNGLCLIENTWAITSYNCPROC glClientWaitSync = nullptr;
        PFNGLDELETESYNCPROC glDeleteSync = nullptr;
        PFNGLGETSTRINGIPROC glGetStringi = nullptr;
    };
} // namespace ase

