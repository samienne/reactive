project('arrange', 'cpp',
  default_options: ['cpp_std=c++17'])

arrange_inc = include_directories('include')

arrange_sources = files(
  'src/constraint.cpp',
  'src/diff.cpp',
  'src/expression.cpp',
  'src/id_counter.cpp',
  'src/solver.cpp',
  'src/strength.cpp',
  'src/variable.cpp',
)

# Static only on request: see include/arrange/export.h.
arrange_static_args = (get_option('default_library') == 'static'
  ? ['-DARRANGE_STATIC'] : [])

arrange_lib = library(
  'arrange',
  arrange_sources,
  include_directories: arrange_inc,
  cpp_args: ['-DARRANGE_BUILD'] + arrange_static_args,
)

arrange_dep = declare_dependency(
  link_with: arrange_lib,
  include_directories: arrange_inc,
  compile_args: arrange_static_args,
)

subdir('test')
