btl_args = []
if get_option('tracy_enable')
  btl_args = ['-DTRACY_ENABLE']
endif

btl_srcs = ['src/runloop.cpp']
if host_machine.system() == 'windows'
  btl_srcs += 'src/win32/runloopimpl.cpp'
else
  btl_srcs += 'src/posix/runloopimpl.cpp'
endif

libbtl = static_library('btl',
  btl_srcs,
  include_directories: include_directories('include', 'src'),
  cpp_args: btl_args,
  pic: true,
  )

libbtldep = declare_dependency(
        link_with: libbtl,
        include_directories: include_directories('include'),
        dependencies: [],
        compile_args: btl_args,
        )

btltest = executable('btltest',
  'test/asynctest.cpp',
  'test/btlcollectiontest.cpp',
  'test/btlreducetest.cpp',
  'test/clonetest.cpp',
  'test/invoketest.cpp',
  'test/lrucachetest.cpp',
  'test/pmrtest.cpp',
  'test/resulttest.cpp',
  'test/runlooptest.cpp',
  'test/sharedtest.cpp',
  'test/threadpooltest.cpp',
  'test/tupletest.cpp',
  'test/typelisttest.cpp',
  'test/uniquetest.cpp',
  'test/utf8test.cpp',
  dependencies: [libbtldep, gtestdep],
  )

test('btl', btltest, timeout : 1200, args: ['--gtest_output=xml'])
