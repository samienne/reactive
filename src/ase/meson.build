
glsrcs = [
  'gl/glplatform.cpp',
  'gl/glprogram.cpp',
  'gl/glshader.cpp',
  'gl/glvertexshader.cpp',
  'gl/glfragmentshader.cpp',
  'gl/glbuffer.cpp',
  'gl/glvertexbuffer.cpp',
  'gl/glindexbuffer.cpp',
  'gl/gluniformbuffer.cpp',
  'gl/gltexture.cpp',
  'gl/glerror.cpp',
  'gl/gltype.cpp',
  'gl/glblendmode.cpp',
  'gl/glrendercontext.cpp',
  'gl/glframebuffer.cpp',
  'gl/glpipeline.cpp',
  'gl/globjectmanager.cpp',
  'gl/glrenderstate.cpp',
  'gl/gldispatchedcontext.cpp',
  'gl/gluniformset.cpp',
  'gl/glrenderbuffer.cpp',
  'gl/glrenderqueue.cpp'
  ]

if target_machine.system() == 'linux'
    gldep = dependency('gl')
    x11dep = dependency('x11')
    xextdep = dependency('xext')
    system_deps = [gldep, x11dep, xextdep]
    system_link_args = []
    system_incs = ['glx']
    system_src = [
        'glx/glxplatform.cpp',
        'glx/glxrendercontext.cpp',
        'glx/glxwindow.cpp',
        'glx/glxcontext.cpp',
        'glx/glxdispatchedcontext.cpp',
        'glx/glxframebuffer.cpp',
        glsrcs
        ]
elif target_machine.system() == 'windows'
    glheaders = subproject('glheaders')
    gldep = dependency('gl')
    glheadersdep = glheaders.get_variable('glheaders_dep')

    system_deps = [glheadersdep, gldep]
    system_link_args = ['shcore.lib', 'winmm.lib']
    system_incs = []
    system_src = [
        'windows/wglplatform.cpp',
        'windows/wglrendercontext.cpp',
        'windows/wglwindow.cpp',
        'windows/wgldispatchedcontext.cpp',
        'windows/wglframebuffer.cpp',
        glsrcs
        ]
else
    system_deps = []
    system_link_args = []
    system_incs = []
    system_src = [
        'dummy/dummyplatform.cpp',
        'dummy/dummyrendercontext.cpp',
        'dummy/dummywindow.cpp',
        'dummy/dummyframebuffer.cpp',
        'dummy/dummyprogram.cpp',
        'dummy/dummyuniformbuffer.cpp',
        'dummy/dummytexture.cpp',
        'dummy/dummypipeline.cpp',
        'dummy/dummyuniformset.cpp',
        'dummy/dummyrenderbuffer.cpp',
        'dummy/dummyrenderqueue.cpp',
    ]
endif

threadsdep = dependency('threads')
asedep = [libeigendep, threadsdep, tracydep]

incdir = include_directories(
        '.',
        '../../include',
        '../../include/ase',
        'gl',
        system_incs
        )

libasesrc = [
        'genericwindow.cpp',
        'platform.cpp',
        'window.cpp',
        'rendercontext.cpp',
        'renderable.cpp',
        'vertexspec.cpp',
        'namedvertexspec.cpp',
        'vertexbuffer.cpp',
        'indexbuffer.cpp',
        'mesh.cpp',
        'dispatcher.cpp',
        'uniformbuffer.cpp',
        'fragmentshader.cpp',
        'vertexshader.cpp',
        'program.cpp',
        'commandbuffer.cpp',
        'drawcommand.cpp',
        'texture.cpp',
        'framebuffer.cpp',
        'pipeline.cpp',
        'pointerbuttonevent.cpp',
        'keyevent.cpp',
        'textevent.cpp',
        'keycode.cpp',
        'uniformset.cpp',
        'renderbuffer.cpp',
        'renderqueue.cpp'
        ]

libase = library('ase',
        [system_src, libasesrc],
        dependencies: [system_deps, asedep],
        cpp_args: ['-DASE_EXPORT_SYMBOLS'],
        link_args: system_link_args,
        gnu_symbol_visibility: 'hidden',
        include_directories: incdir
        )

libasedep = declare_dependency(
        link_with: libase,
        dependencies: asedep,
        include_directories: include_directories('../../include')
        )

