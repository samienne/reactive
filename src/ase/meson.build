
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
  'gl/gluniformset.cpp'
  ]

if target_machine.system() == 'linux'
    gldep = dependency('gl')
    x11dep = dependency('x11')
    xextdep = dependency('xext')
    system_deps = [gldep, x11dep, xextdep]
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
    system_deps = []
    system_incs = []
    system_src = [
        'windows/windowsplatform.cpp',
        'windows/windowsrendercontext.cpp',
        'windows/windowswindow.cpp',
        'dummy/dummyframebuffer.cpp',
        'dummy/dummyprogram.cpp',
        ]
else
    system_deps = []
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
    ]
endif

threadsdep = dependency('threads')
asedep = [libeigendep, threadsdep]

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
        'rendercommand.cpp',
        'renderqueue.cpp',
        'texture.cpp',
        'framebuffer.cpp',
        'pipeline.cpp',
        'pointerbuttonevent.cpp',
        'keyevent.cpp',
        'keycode.cpp',
        'uniformset.cpp'
        ]

libase = library('ase',
        [system_src, libasesrc],
        dependencies: [system_deps, asedep],
        cpp_args: ['-DASE_EXPORT_SYMBOLS'],
        gnu_symbol_visibility: 'hidden',
        include_directories: incdir
        )

libasedep = declare_dependency(
        link_with: libase,
        dependencies: asedep,
        include_directories: include_directories('../../include')
        )

