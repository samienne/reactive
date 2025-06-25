incdir = include_directories('../include')

unittest = executable('unittest',
        'test1.cpp',
        'hashtest.cpp',
        include_directories : incdir,
        link_with : [],
        dependencies : [gtestdep, libasedep, libavgdep, libbquidep])

test('unittest', unittest, timeout : 1200, args: ['--gtest_output=xml'])

