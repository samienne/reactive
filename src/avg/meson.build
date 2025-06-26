libavgsrc = [
  'src/curve/curves.cpp',
  'src/animated.cpp',
  'src/lerp.cpp',
  'src/drawcontext.cpp',
  'src/rendertree.cpp',
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
  'src/clipper/clipper.cpp',
  ]

libavg = library(
  'avg',
  libavgsrc,
  include_directories: include_directories('include', 'include/avg', 'src'),
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
  'test/fonttest.cpp',
  'test/hashtest.cpp',
  'test/pathtest.cpp',
  'test/pentest.cpp',
  dependencies: [gtestdep, libavgdep],
  )

test('avg', avgtest, timeout: 1200, args: ['--gtest_output=xml'])

