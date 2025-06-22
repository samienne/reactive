incdir = include_directories('../include')

unittest = executable('unittest',
        'widgetinstancemodifiertest.cpp',
        'databindtest.cpp',
        'widgettest.cpp',
        'resulttest.cpp',
        'test1.cpp',
        'hashtest.cpp',
        'collectiontest.cpp',
        include_directories : incdir,
        link_with : [],
        dependencies : [gtestdep, libasedep, libavgdep, libreactivedep])

test('unittest', unittest, timeout : 1200, args: ['--gtest_output=xml'])

