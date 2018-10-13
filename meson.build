project('ghosd', 'c',
        default_options: ['c_std=c99'])
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

version=run_command('cat', 'VERSION').stdout().strip()
git = find_program('git', required: false)
if git.found()
        vcmd = run_command(git, 'describe')
        if vcmd.returncode() == 0
                version=vcmd.stdout().strip()
        endif
endif
add_project_arguments('-DVERSION="' + version + '"', language: 'c')

cc = meson.get_compiler('c')
rt_dep = cc.find_library('rt')
xcb_ewmh_dep = dependency('xcb-ewmh')
cairo_dep = dependency('cairo')
pango_dep = dependency('pangocairo')
thread_dep = dependency('threads')

src = ['src/ghosd.c', 'src/draw.c', 'src/helper.c']
executable('ghosd', sources: src, install: true,
dependencies: [rt_dep, xcb_ewmh_dep, cairo_dep, pango_dep, thread_dep])

a2x = find_program('a2x', required: false)
if a2x.found()
        man = custom_target('doc',
        output: 'ghosd.1',
        input: 'doc/ghosd.1.asciidoc',
        command: [a2x, '-v', '-d', 'manpage', '-f', 'manpage', '-a',
        'revnumber=' + version, '@INPUT@']
        )
endif

install_man('doc/ghosd.1')
