#include "glxdispatchedcontext.h"

#include "glxplatform.h"
#include "glxwindow.h"

#include <cassert>

namespace ase
{

namespace
{
    GlFunctions getGlFunctionPointers()
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
        gl.glActiveTexture = (PFNGLACTIVETEXTUREPROC)
            glXGetProcAddressARB((GLubyte const*)"glActiveTexture");
        gl.glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)
            glXGetProcAddressARB((GLubyte const*)"glGenRenderbuffers");
        gl.glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)
            glXGetProcAddressARB((GLubyte const*)"glRenderbufferStorage");
        gl.glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)
            glXGetProcAddressARB((GLubyte const*)"glDeleteRenderbuffers");
        gl.glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)
            glXGetProcAddressARB((GLubyte const*)"glBindRenderbuffer");
        gl.glFenceSync = (PFNGLFENCESYNCPROC)
            glXGetProcAddressARB((GLubyte const*)"glFenceSync");
        gl.glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)
            glXGetProcAddressARB((GLubyte const*)"glClientWaitSync");
        gl.glDeleteSync = (PFNGLDELETESYNCPROC)
            glXGetProcAddressARB((GLubyte const*)"glDeleteSync");
        gl.glGetStringi = (PFNGLGETSTRINGIPROC)
            glXGetProcAddressARB((GLubyte const*)"glGetStringi");

        return gl;
    }
} // Anonymous namespace

GlxDispatchedContext::GlxDispatchedContext(GlxPlatform& platform) :
    GlDispatchedContext(),
    platform_(platform),
    context_(platform),
    currentWindow_(0)
{
    dispatcher_.run([this]()
    {
        context_.makeCurrent(platform_.lockX(), 0);
        setGlFunctions(getGlFunctionPointers());
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

void GlxDispatchedContext::makeCurrent(Dispatched, GlxWindow const& window)
{
    if (currentWindow_ != window.getGlxWindowId())
    {
        currentWindow_ = window.getGlxWindowId();
        context_.makeCurrent(platform_.lockX(), currentWindow_);
    }
}

} // namespace ase

