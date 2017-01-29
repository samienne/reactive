
incdir = include_directories('../../include', '../../include/avg', '.')

libavgsrc = [
    'fontmanager.cpp',
    'fontimpl.cpp',
    'font.cpp',
    'region.cpp',
    'rect.cpp',
    'simplepolygon.cpp',
    'path.cpp',
    'pathspec.cpp',
    'painter.cpp',
    'drawing.cpp',
    'shape.cpp',
    'pen.cpp',
    'softmesh.cpp',
    'textentry.cpp',
    'obb.cpp',
    'clipper/clipper.cpp',
    'poly2tri/common/shapes.cc',
    'poly2tri/sweep/advancing_front.cc',
    'poly2tri/sweep/cdt.cc',
    'poly2tri/sweep/sweep.cc',
    'poly2tri/sweep/sweep_context.cc']

libavg = library(
        'avg',
        libavgsrc,
        include_directories : incdir,
        dependencies : [libasedep, libfreetypedep])

libavgdep = declare_dependency(
        link_with: libavg,
        dependencies: [libasedep, libfreetypedep],
        include_directories: include_directories('../../include')
        )

