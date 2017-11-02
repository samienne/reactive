project('ase', 'cpp')
add_global_arguments('-std=c++17', '-pedantic', '-Wextra', '-Wall',
        '-Wno-maybe-uninitialized', '-Wno-ignored-attributes', language : 'cpp')
#'-flto',

range = subproject('range')
librangedep = range.get_variable('range_dep')

eigen = subproject('eigen')
libeigendep = eigen.get_variable('eigen_dep')

freetype = subproject('freetype')
libfreetypedep = freetype.get_variable('freetype_dep')
#libfreetypedep = dependency('freetype2')

subdir('src')
#subdir('test')

