project('mapbox', 'cpp', license: 'isc')

incdir = include_directories('include')

install_headers(files('include/mapbox/earcut.hpp'), subdir: 'mapbox')

mapbox_dep = declare_dependency(
  include_directories: incdir
  )
