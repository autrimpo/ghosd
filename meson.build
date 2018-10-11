project('ghosd', 'c',
        default_options: ['c_std=c99'])
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')
cc = meson.get_compiler('c')
rt_dep = cc.find_library('rt')
xcb_ewmh_dep = dependency('xcb-ewmh')
cairo_dep = dependency('cairo')
pango_dep = dependency('pangocairo')
thread_dep = dependency('threads')
src = ['src/ghosd.c', 'src/draw.c', 'src/helper.c']
executable('ghosd', sources: src, dependencies: [rt_dep, xcb_ewmh_dep, cairo_dep, pango_dep, thread_dep])
