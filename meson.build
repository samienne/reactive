project('ase', 'cpp')
add_global_arguments('-std=c++17', '-pedantic', '-Wextra', '-Wall',
        '-fvisibility=hidden', language : 'cpp')
#'-flto',

eigen = subproject('eigen')
libeigendep = eigen.get_variable('eigen_dep')

freetype = subproject('freetype2')
libfreetypedep = freetype.get_variable('freetype_dep')

subdir('src')
subdir('test')

