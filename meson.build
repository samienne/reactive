project('ase', 'cpp')

if target_machine.system() == 'linux'
    add_global_arguments('-std=c++17', '-pedantic', '-Wextra', '-Wall',
            language : 'cpp')
else
    add_global_arguments('/std:c++17', '/wd4251', language : 'cpp')
endif

#'-flto','-fvisibility=hidden',
# '-Wextra', 

eigen = subproject('eigen')
libeigendep = eigen.get_variable('eigen_dep')

freetype = subproject('freetype2')
libfreetypedep = freetype.get_variable('freetype_dep')

subdir('src')
subdir('test')

