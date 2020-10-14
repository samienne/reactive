#include "wgldispatchedcontext.h"

#include "wglplatform.h"

#include <cassert>

namespace ase
{

namespace
{
    GlFunctions getGlFunctions()
    {
        GlFunctions gl;

        gl.glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)
            wglGetProcAddress("glVertexAttribPointer");
        gl.glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)
            wglGetProcAddress("glDisableVertexAttribArray");
        gl.glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)
            wglGetProcAddress("glEnableVertexAttribArray");
        gl.glUniform1fv = (PFNGLUNIFORM1FVPROC)
            wglGetProcAddress("glUniform1fv");
        gl.glUniform2fv = (PFNGLUNIFORM2FVPROC)
            wglGetProcAddress("glUniform2fv");
        gl.glUniform3fv = (PFNGLUNIFORM3FVPROC)
            wglGetProcAddress("glUniform3fv");
        gl.glUniform4fv = (PFNGLUNIFORM4FVPROC)
            wglGetProcAddress("glUniform4fv");
        gl.glUniform1iv = (PFNGLUNIFORM1IVPROC)
            wglGetProcAddress("glUniform1iv");
        gl.glUniform2iv = (PFNGLUNIFORM2IVPROC)
            wglGetProcAddress("glUniform2iv");
        gl.glUniform3iv = (PFNGLUNIFORM3IVPROC)
            wglGetProcAddress("glUniform3iv");
        gl.glUniform4iv = (PFNGLUNIFORM4IVPROC)
            wglGetProcAddress("glUniform4iv");
        gl.glUniform1uiv = (PFNGLUNIFORM1UIVPROC)
            wglGetProcAddress("glUniform1uiv");
        gl.glUniform2uiv = (PFNGLUNIFORM2UIVPROC)
            wglGetProcAddress("glUniform2uiv");
        gl.glUniform3uiv = (PFNGLUNIFORM3UIVPROC)
            wglGetProcAddress("glUniform3uiv");
        gl.glUniform4uiv = (PFNGLUNIFORM4UIVPROC)
            wglGetProcAddress("glUniform4uiv");
        gl.glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)
            wglGetProcAddress("glUniformMatrix4fv");
        gl.glUseProgram = (PFNGLUSEPROGRAMPROC)
            wglGetProcAddress("glUseProgram");
        gl.glBindBuffer = (PFNGLBINDBUFFERPROC)
            wglGetProcAddress("glBindBuffer");
        gl.glGenBuffers = (PFNGLGENBUFFERSPROC)
            wglGetProcAddress("glGenBuffers");
        gl.glBufferData = (PFNGLBUFFERDATAPROC)
            wglGetProcAddress("glBufferData");
        gl.glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)
            wglGetProcAddress("glDeleteBuffers");
        gl.glCreateProgram = (PFNGLCREATEPROGRAMPROC)
            wglGetProcAddress("glCreateProgram");
        gl.glAttachShader = (PFNGLATTACHSHADERPROC)
            wglGetProcAddress("glAttachShader");
        gl.glLinkProgram = (PFNGLLINKPROGRAMPROC)
            wglGetProcAddress("glLinkProgram");
        gl.glGetProgramiv = (PFNGLGETPROGRAMIVPROC)
            wglGetProcAddress("glGetProgramiv");
        gl.glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)
            wglGetProcAddress("glGetProgramInfoLog");
        gl.glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)
            wglGetProcAddress("glGetActiveAttrib");
        gl.glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)
            wglGetProcAddress("glGetAttribLocation");
        gl.glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)
            wglGetProcAddress("glGetActiveUniform");
        gl.glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)
            wglGetProcAddress("glGetUniformLocation");
        gl.glDeleteProgram = (PFNGLDELETEPROGRAMPROC)
            wglGetProcAddress("glDeleteProgram");
        gl.glCreateShader = (PFNGLCREATESHADERPROC)
            wglGetProcAddress("glCreateShader");
        gl.glShaderSource = (PFNGLSHADERSOURCEPROC)
            wglGetProcAddress("glShaderSource");
        gl.glCompileShader = (PFNGLCOMPILESHADERPROC)
            wglGetProcAddress("glCompileShader");
        gl.glGetShaderiv = (PFNGLGETSHADERIVPROC)
            wglGetProcAddress("glGetShaderiv");
        gl.glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)
            wglGetProcAddress("glGetShaderInfoLog");
        gl.glDeleteShader = (PFNGLDELETESHADERPROC)
            wglGetProcAddress("glDeleteShader");
        gl.glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)
            wglGetProcAddress("glGenFramebuffers");
        gl.glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)
            wglGetProcAddress("glDeleteFramebuffers");
        gl.glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)
            wglGetProcAddress("glFramebufferTexture2D");
        gl.glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)
            wglGetProcAddress("glBindFramebuffer");
        gl.glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)
            wglGetProcAddress("glGenVertexArrays");
        gl.glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)
            wglGetProcAddress("glBindVertexArray");
        gl.glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)
            wglGetProcAddress("glDeleteVertexArrays");
        gl.glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)
            wglGetProcAddress("glBindBufferRange");
        gl.glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)
            wglGetProcAddress("glGetUniformBlockIndex");
        gl.glActiveTexture = (PFNGLACTIVETEXTUREPROC)
            wglGetProcAddress("glActiveTexture");

        return gl;
    }
} // Anonymous namespace

WglDispatchedContext::WglDispatchedContext(WglPlatform& platform) :
    GlDispatchedContext(),
    platform_(platform),
    context_(platform_.createRawContext(2, 0))
{
    dispatcher_.run([this]()
    {
        wglMakeCurrent(platform_.getDummyDc(), context_);
        setGlFunctions(getGlFunctions());
    });

    dispatcher_.wait();
}

HGLRC WglDispatchedContext::getWglContext() const
{
    return context_;
}

} // namespace ase

