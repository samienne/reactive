project('ase', 'cpp', default_options: ['cpp_std=c++17'])

if target_machine.system() == 'linux'
    add_global_arguments('-pedantic', '-Wextra', '-Wall', language : 'cpp')
elif target_machine.system() == 'windows'
    add_global_arguments('/wd4251', '/bigobj', language : 'cpp')
endif

eigen = subproject('eigen')
libeigendep = eigen.get_variable('eigen_dep')

freetype = subproject('freetype2')
libfreetypedep = freetype.get_variable('freetype_dep')

subdir('src')
subdir('test')

