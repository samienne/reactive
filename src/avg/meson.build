libavgsrc = [
  'src/curve/curves.cpp',
  'src/animated.cpp',
  'src/lerp.cpp',
  'src/drawcontext.cpp',
  'src/rendertree/uniqueid.cpp',
  'src/rendertree/updateresult.cpp',
  'src/rendertree/rendertreenode.cpp',
  'src/rendertree/containernode.cpp',
  'src/rendertree/rectnode.cpp',
  'src/rendertree/transitionnode.cpp',
  'src/rendertree/clipnode.cpp',
  'src/rendertree/idnode.cpp',
  'src/rendertree/rendertree.cpp',
  'src/fontmanager.cpp',
  'src/fontimpl.cpp',
  'src/font.cpp',
  'src/region.cpp',
  'src/rect.cpp',
  'src/polyline.cpp',
  'src/path.cpp',
  'src/pathbuilder.cpp',
  'src/painter.cpp',
  'src/drawing.cpp',
  'src/shape.cpp',
  'src/pen.cpp',
  'src/softmesh.cpp',
  'src/textentry.cpp',
  'src/obb.cpp',
  'src/rendering.cpp',
  'src/targetimage.cpp',
  ]

# Clipper is vendored third-party code. It builds as its own target so its
# diagnostics are silenced without lowering the warning level of code we wrote.
# warning_level=0 alone only drops the /W flag; the explicit flag is what
# silences the compiler's default-on diagnostics.
if meson.get_compiler('cpp').get_argument_syntax() == 'msvc'
  clipper_args = ['/W0']
else
  clipper_args = ['-w']
endif

libclipper = static_library(
  'clipper',
  'src/clipper/clipper.cpp',
  cpp_args: clipper_args,
  dependencies: [libbtldep, libeigendep],
  override_options: ['warning_level=0'],
  gnu_symbol_visibility: 'hidden',
  pic: true
  )

libavg = library(
  'avg',
  libavgsrc,
  include_directories: include_directories('include', 'include/avg', 'src'),
  link_with: libclipper,
  dependencies: [libasedep, libfreetypedep, mapboxdep],
  cpp_args: ['-DAVG_EXPORT_SYMBOLS'],
  gnu_symbol_visibility: 'hidden'
  )

libavgdep = declare_dependency(
  link_with: libavg,
  dependencies: [libasedep, libfreetypedep],
  include_directories: include_directories('include')
  )

avgtest =  executable('avgtest',
  'test/animatedtest.cpp',
  'test/avgtransformtest.cpp',
  'test/brushtest.cpp',
  'test/colortest.cpp',
  'test/drawtest.cpp',
  'test/fonttest.cpp',
  'test/hashtest.cpp',
  'test/pathtest.cpp',
  'test/pentest.cpp',
  dependencies: [gtestdep, libavgdep],
  )

test('avg', avgtest, timeout: 1200,
  args: ['--gtest_output=xml:' + meson.current_build_dir() / 'test_detail.xml'],
  workdir: meson.project_source_root())

