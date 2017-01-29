incdir = include_directories('../../include')

testapp2 = executable(
        'testapp2',
        'main.cpp',
        include_directories : incdir,
        link_with : [libase, libavg, libreactive],
        dependencies : asedep
        )
