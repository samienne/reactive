btl_args = []
if get_option('tracy_enable')
  btl_args += ['-DTRACY_ENABLE']
endif

btl_args = ['-fcoroutines-ts']

libbtldep = declare_dependency(
        include_directories: include_directories('../include'),
        dependencies: [],
        compile_args: btl_args,
        )

subdir('ase')
subdir('avg')
subdir('reactive')
subdir('testapp1')

