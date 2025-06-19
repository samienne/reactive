incdir = include_directories('../include')

unittest = executable('unittest',
        'asynctest.cpp',
        'animatedtest.cpp',
        'widgetinstancemodifiertest.cpp',
        'pmrtest.cpp',
        'databindtest.cpp',
        'btlreducetest.cpp',
        'btlcollectiontest.cpp',
        'collectiontest.cpp',
        'clonetest.cpp',
        'widgettest.cpp',
        'resulttest.cpp',
        'tupletest.cpp',
        'utf8test.cpp',
        'test1.cpp',
        'pathtest.cpp',
        'avgtransformtest.cpp',
        'fonttest.cpp',
        'hashtest.cpp',
        'sharedtest.cpp',
        'uniquetest.cpp',
        'brushtest.cpp',
        'colortest.cpp',
        'pentest.cpp',
        'typelisttest.cpp',
        'invoketest.cpp',
        include_directories : incdir,
        link_with : [],
        dependencies : [gtestdep, libasedep, libavgdep, libreactivedep])

test('unittest', unittest, timeout : 1200, args: ['--gtest_output=xml'])

