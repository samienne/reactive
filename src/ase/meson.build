
glsrcs = [
  'src/gl/glplatform.cpp',
  'src/gl/glprogram.cpp',
  'src/gl/glshader.cpp',
  'src/gl/glvertexshader.cpp',
  'src/gl/glfragmentshader.cpp',
  'src/gl/glbuffer.cpp',
  'src/gl/glvertexbuffer.cpp',
  'src/gl/glindexbuffer.cpp',
  'src/gl/gluniformbuffer.cpp',
  'src/gl/gltexture.cpp',
  'src/gl/glerror.cpp',
  'src/gl/gltype.cpp',
  'src/gl/glblendmode.cpp',
  'src/gl/glrendercontext.cpp',
  'src/gl/glframebuffer.cpp',
  'src/gl/glpipeline.cpp',
  'src/gl/globjectmanager.cpp',
  'src/gl/glrenderstate.cpp',
  'src/gl/gldispatchedcontext.cpp',
  'src/gl/gluniformset.cpp',
  'src/gl/glrenderbuffer.cpp',
  'src/gl/glrenderqueue.cpp'
  ]

if target_machine.system() == 'linux'
  gldep = dependency('gl')
  x11dep = dependency('x11')
  xextdep = dependency('xext')
  system_deps = [gldep, x11dep, xextdep]
  system_link_args = []
  system_incs = ['src/glx']
  system_src = [
    'src/glx/glxplatform.cpp',
    'src/glx/glxrendercontext.cpp',
    'src/glx/glxwindow.cpp',
    'src/glx/glxcontext.cpp',
    'src/glx/glxdispatchedcontext.cpp',
    'src/glx/glxframebuffer.cpp',
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
    'src/windows/wglplatform.cpp',
    'src/windows/wglrendercontext.cpp',
    'src/windows/wglwindow.cpp',
    'src/windows/wgldispatchedcontext.cpp',
    'src/windows/wglframebuffer.cpp',
    glsrcs
    ]
else
  system_deps = []
  system_link_args = []
  system_incs = []
  system_src = [
    'src/dummy/dummyplatform.cpp',
    'src/dummy/dummyrendercontext.cpp',
    'src/dummy/dummywindow.cpp',
    'src/dummy/dummyframebuffer.cpp',
    'src/dummy/dummyprogram.cpp',
    'src/dummy/dummyuniformbuffer.cpp',
    'src/dummy/dummytexture.cpp',
    'src/dummy/dummypipeline.cpp',
    'src/dummy/dummyuniformset.cpp',
    'src/dummy/dummyrenderbuffer.cpp',
    'src/dummy/dummyrenderqueue.cpp',
    ]
endif

threadsdep = dependency('threads')
asedep = [libeigendep, threadsdep, tracydep, libbtldep]

incdir = include_directories(
  'src',
  'include',
  'include/ase',
  'src/gl',
  system_incs
  )

libasesrc = [
  'src/genericwindow.cpp',
  'src/platform.cpp',
  'src/window.cpp',
  'src/rendercontext.cpp',
  'src/renderable.cpp',
  'src/vertexspec.cpp',
  'src/namedvertexspec.cpp',
  'src/vertexbuffer.cpp',
  'src/indexbuffer.cpp',
  'src/mesh.cpp',
  'src/dispatcher.cpp',
  'src/uniformbuffer.cpp',
  'src/fragmentshader.cpp',
  'src/vertexshader.cpp',
  'src/program.cpp',
  'src/commandbuffer.cpp',
  'src/drawcommand.cpp',
  'src/texture.cpp',
  'src/framebuffer.cpp',
  'src/pipeline.cpp',
  'src/pointerbuttonevent.cpp',
  'src/keyevent.cpp',
  'src/textevent.cpp',
  'src/keycode.cpp',
  'src/uniformset.cpp',
  'src/renderbuffer.cpp',
  'src/renderqueue.cpp'
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
  include_directories: include_directories('include')
  )

