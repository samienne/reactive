#include "glxdispatchedcontext.h"

#include "glxplatform.h"

#include <cassert>

namespace ase
{

namespace
{
    GlFunctions getGlFunctions()
    {
        GlFunctions gl;

        gl.glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)
            glXGetProcAddressARB((GLubyte const*)"glVertexAttribPointer");
        gl.glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)
            glXGetProcAddressARB((GLubyte const*)"glDisableVertexAttribArray");
        gl.glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)
            glXGetProcAddressARB((GLubyte const*)"glEnableVertexAttribArray");
        gl.glUniform1fv = (PFNGLUNIFORM1FVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform1fv");
        gl.glUniform2fv = (PFNGLUNIFORM2FVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform2fv");
        gl.glUniform3fv = (PFNGLUNIFORM3FVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform3fv");
        gl.glUniform4fv = (PFNGLUNIFORM4FVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform4fv");
        gl.glUniform1iv = (PFNGLUNIFORM1IVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform1iv");
        gl.glUniform2iv = (PFNGLUNIFORM2IVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform2iv");
        gl.glUniform3iv = (PFNGLUNIFORM3IVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform3iv");
        gl.glUniform4iv = (PFNGLUNIFORM4IVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform4iv");
        gl.glUniform1uiv = (PFNGLUNIFORM1UIVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform1uiv");
        gl.glUniform2uiv = (PFNGLUNIFORM2UIVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform2uiv");
        gl.glUniform3uiv = (PFNGLUNIFORM3UIVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform3uiv");
        gl.glUniform4uiv = (PFNGLUNIFORM4UIVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniform4uiv");
        gl.glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)
            glXGetProcAddressARB((GLubyte const*)"glUniformMatrix4fv");
        gl.glUseProgram = (PFNGLUSEPROGRAMPROC)
            glXGetProcAddressARB((GLubyte const*)"glUseProgram");
        gl.glBindBuffer = (PFNGLBINDBUFFERPROC)
            glXGetProcAddressARB((GLubyte const*)"glBindBuffer");
        gl.glGenBuffers = (PFNGLGENBUFFERSPROC)
            glXGetProcAddressARB((GLubyte const*)"glGenBuffers");
        gl.glBufferData = (PFNGLBUFFERDATAPROC)
            glXGetProcAddressARB((GLubyte const*)"glBufferData");
        gl.glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)
            glXGetProcAddressARB((GLubyte const*)"glDeleteBuffers");
        gl.glCreateProgram = (PFNGLCREATEPROGRAMPROC)
            glXGetProcAddressARB((GLubyte const*)"glCreateProgram");
        gl.glAttachShader = (PFNGLATTACHSHADERPROC)
            glXGetProcAddressARB((GLubyte const*)"glAttachShader");
        gl.glLinkProgram = (PFNGLLINKPROGRAMPROC)
            glXGetProcAddressARB((GLubyte const*)"glLinkProgram");
        gl.glGetProgramiv = (PFNGLGETPROGRAMIVPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetProgramiv");
        gl.glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetProgramInfoLog");
        gl.glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetActiveAttrib");
        gl.glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetAttribLocation");
        gl.glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetActiveUniform");
        gl.glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetUniformLocation");
        gl.glDeleteProgram = (PFNGLDELETEPROGRAMPROC)
            glXGetProcAddressARB((GLubyte const*)"glDeleteProgram");
        gl.glCreateShader = (PFNGLCREATESHADERPROC)
            glXGetProcAddressARB((GLubyte const*)"glCreateShader");
        gl.glShaderSource = (PFNGLSHADERSOURCEPROC)
            glXGetProcAddressARB((GLubyte const*)"glShaderSource");
        gl.glCompileShader = (PFNGLCOMPILESHADERPROC)
            glXGetProcAddressARB((GLubyte const*)"glCompileShader");
        gl.glGetShaderiv = (PFNGLGETSHADERIVPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetShaderiv");
        gl.glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetShaderInfoLog");
        gl.glDeleteShader = (PFNGLDELETESHADERPROC)
            glXGetProcAddressARB((GLubyte const*)"glDeleteShader");
        gl.glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)
            glXGetProcAddressARB((GLubyte const*)"glGenFramebuffers");
        gl.glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)
            glXGetProcAddressARB((GLubyte const*)"glDeleteFramebuffers");
        gl.glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)
            glXGetProcAddressARB((GLubyte const*)"glFramebufferTexture2D");
        gl.glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)
            glXGetProcAddressARB((GLubyte const*)"glBindFramebuffer");
        gl.glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)
            glXGetProcAddressARB((GLubyte const*)"glGenVertexArrays");
        gl.glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)
            glXGetProcAddressARB((GLubyte const*)"glBindVertexArray");
        gl.glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)
            glXGetProcAddressARB((GLubyte const*)"glDeleteVertexArrays");
        gl.glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)
            glXGetProcAddressARB((GLubyte const*)"glBindBufferRange");
        gl.glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetUniformBlockIndex");

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

        return gl;
    }
} // Anonymous namespace

GlxDispatchedContext::GlxDispatchedContext(GlxPlatform& platform) :
    GlDispatchedContext(),
    platform_(platform),
    context_(platform)
{
    dispatcher_.run([this]()
    {
        context_.makeCurrent(platform_.lockX(), 0);
        gl_ = ase::getGlFunctions();
    });

    dispatcher_.wait();
}

GlxContext const& GlxDispatchedContext::getGlxContext() const
{
    return context_;
}

GlxContext& GlxDispatchedContext::getGlxContext()
{
    return context_;
}

} // namespace ase

