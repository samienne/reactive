incdir = include_directories('../include')

unittest = executable('unittest',
        'animatedtest.cpp',
        'widgetinstancemodifiertest.cpp',
        'databindtest.cpp',
        'widgettest.cpp',
        'resulttest.cpp',
        'test1.cpp',
        'pathtest.cpp',
        'avgtransformtest.cpp',
        'fonttest.cpp',
        'brushtest.cpp',
        'colortest.cpp',
        'pentest.cpp',
        'hashtest.cpp',
        'collectiontest.cpp',
        include_directories : incdir,
        link_with : [],
        dependencies : [gtestdep, libasedep, libavgdep, libreactivedep])

test('unittest', unittest, timeout : 1200, args: ['--gtest_output=xml'])

