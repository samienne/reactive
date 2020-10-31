#include "wgldispatchedcontext.h"

#include "wglplatform.h"

#include <iostream>
#include <cassert>

namespace ase
{

namespace
{
    void* getProcAddress(char const* name)
    {
        void* p = (void*)wglGetProcAddress(name);

        if (p == nullptr || p == (void*)0x1 || p == (void*)0x2
                || p == (void*)0x3 || p == (void*)-1)
        {
            HMODULE module = LoadLibraryA("opengl32.dll");
            p = (void*)GetProcAddress(module, name);
            std::cout << name << ": " << p << std::endl;
        }

        return p;
    }

    GlFunctions getGlFunctionPointers()
    {
        GlFunctions gl;

        gl.glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)
            getProcAddress("glVertexAttribPointer");
        gl.glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)
            getProcAddress("glDisableVertexAttribArray");
        gl.glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)
            getProcAddress("glEnableVertexAttribArray");
        gl.glUniform1fv = (PFNGLUNIFORM1FVPROC)
            getProcAddress("glUniform1fv");
        gl.glUniform2fv = (PFNGLUNIFORM2FVPROC)
            getProcAddress("glUniform2fv");
        gl.glUniform3fv = (PFNGLUNIFORM3FVPROC)
            getProcAddress("glUniform3fv");
        gl.glUniform4fv = (PFNGLUNIFORM4FVPROC)
            getProcAddress("glUniform4fv");
        gl.glUniform1iv = (PFNGLUNIFORM1IVPROC)
            getProcAddress("glUniform1iv");
        gl.glUniform2iv = (PFNGLUNIFORM2IVPROC)
            getProcAddress("glUniform2iv");
        gl.glUniform3iv = (PFNGLUNIFORM3IVPROC)
            getProcAddress("glUniform3iv");
        gl.glUniform4iv = (PFNGLUNIFORM4IVPROC)
            getProcAddress("glUniform4iv");
        gl.glUniform1uiv = (PFNGLUNIFORM1UIVPROC)
            getProcAddress("glUniform1uiv");
        gl.glUniform2uiv = (PFNGLUNIFORM2UIVPROC)
            getProcAddress("glUniform2uiv");
        gl.glUniform3uiv = (PFNGLUNIFORM3UIVPROC)
            getProcAddress("glUniform3uiv");
        gl.glUniform4uiv = (PFNGLUNIFORM4UIVPROC)
            getProcAddress("glUniform4uiv");
        gl.glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)
            getProcAddress("glUniformMatrix4fv");
        gl.glUseProgram = (PFNGLUSEPROGRAMPROC)
            getProcAddress("glUseProgram");
        gl.glBindBuffer = (PFNGLBINDBUFFERPROC)
            getProcAddress("glBindBuffer");
        gl.glGenBuffers = (PFNGLGENBUFFERSPROC)
            getProcAddress("glGenBuffers");
        gl.glBufferData = (PFNGLBUFFERDATAPROC)
            getProcAddress("glBufferData");
        gl.glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)
            getProcAddress("glDeleteBuffers");
        gl.glCreateProgram = (PFNGLCREATEPROGRAMPROC)
            getProcAddress("glCreateProgram");
        gl.glAttachShader = (PFNGLATTACHSHADERPROC)
            getProcAddress("glAttachShader");
        gl.glLinkProgram = (PFNGLLINKPROGRAMPROC)
            getProcAddress("glLinkProgram");
        gl.glGetProgramiv = (PFNGLGETPROGRAMIVPROC)
            getProcAddress("glGetProgramiv");
        gl.glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)
            getProcAddress("glGetProgramInfoLog");
        gl.glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)
            getProcAddress("glGetActiveAttrib");
        gl.glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)
            getProcAddress("glGetAttribLocation");
        gl.glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)
            getProcAddress("glGetActiveUniform");
        gl.glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)
            getProcAddress("glGetUniformLocation");
        gl.glDeleteProgram = (PFNGLDELETEPROGRAMPROC)
            getProcAddress("glDeleteProgram");
        gl.glCreateShader = (PFNGLCREATESHADERPROC)
            getProcAddress("glCreateShader");
        gl.glShaderSource = (PFNGLSHADERSOURCEPROC)
            getProcAddress("glShaderSource");
        gl.glCompileShader = (PFNGLCOMPILESHADERPROC)
            getProcAddress("glCompileShader");
        gl.glGetShaderiv = (PFNGLGETSHADERIVPROC)
            getProcAddress("glGetShaderiv");
        gl.glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)
            getProcAddress("glGetShaderInfoLog");
        gl.glDeleteShader = (PFNGLDELETESHADERPROC)
            getProcAddress("glDeleteShader");
        gl.glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)
            getProcAddress("glGenFramebuffers");
        gl.glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)
            getProcAddress("glDeleteFramebuffers");
        gl.glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)
            getProcAddress("glFramebufferTexture2D");
        gl.glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)
            getProcAddress("glBindFramebuffer");
        gl.glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)
            getProcAddress("glGenVertexArrays");
        gl.glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)
            getProcAddress("glBindVertexArray");
        gl.glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)
            getProcAddress("glDeleteVertexArrays");
        gl.glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)
            getProcAddress("glBindBufferRange");
        gl.glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)
            getProcAddress("glGetUniformBlockIndex");
        gl.glActiveTexture = (PFNGLACTIVETEXTUREPROC)
            getProcAddress("glActiveTexture");

        return gl;
    }
} // Anonymous namespace

WglDispatchedContext::WglDispatchedContext(WglPlatform& platform,
        HGLRC context) :
    GlDispatchedContext(),
    platform_(platform),
    context_(context)
{
    if (!context_)
        return;

    dispatcher_.run([this]()
    {
        wglMakeCurrent(platform_.getDummyDc(), context_);
        setGlFunctions(getGlFunctionPointers());
    });

    dispatcher_.wait();
}

HGLRC WglDispatchedContext::getWglContext() const
{
    return context_;
}

} // namespace ase

