project('arrange', 'cpp',
  default_options: ['cpp_std=c++17', 'default_library=static'])

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

arrange_lib = library(
  'arrange',
  arrange_sources,
  include_directories: arrange_inc,
)

arrange_dep = declare_dependency(
  link_with: arrange_lib,
  include_directories: arrange_inc,
)
