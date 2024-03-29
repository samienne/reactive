
incdir = include_directories('../../include', '../../include/avg', '.')

libavgsrc = [
    'curve/curves.cpp',
    'animated.cpp',
    'lerp.cpp',
    'drawcontext.cpp',
    'drawnode.cpp',
    'rendertree.cpp',
    'fontmanager.cpp',
    'fontimpl.cpp',
    'font.cpp',
    'region.cpp',
    'rect.cpp',
    'polyline.cpp',
    'path.cpp',
    'pathbuilder.cpp',
    'painter.cpp',
    'drawing.cpp',
    'shape.cpp',
    'pen.cpp',
    'softmesh.cpp',
    'textentry.cpp',
    'obb.cpp',
    'rendering.cpp',
    'targetimage.cpp',
    'clipper/clipper.cpp',
    ]

libavg = library(
        'avg',
        libavgsrc,
        include_directories : incdir,
        dependencies : [libasedep, libfreetypedep, mapboxdep],
        cpp_args: ['-DAVG_EXPORT_SYMBOLS'],
        gnu_symbol_visibility: 'hidden'
        )

libavgdep = declare_dependency(
        link_with: libavg,
        dependencies: [libasedep, libfreetypedep],
        include_directories: include_directories('../../include')
        )

