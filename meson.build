project('io.elementary.ubuntu-package-imports', 'vala', 'c')

glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
gobject_dep = dependency('gobject-2.0')
gee_dep = dependency('gee-0.8')
git2_glib_dep = dependency('libgit2-glib-1.0')
ggit_dep = meson.get_compiler('vala').find_library('ggit-1.0')
git2_glib_c_dep = meson.get_compiler('c').find_library('libgit2-glib-1.0')

dependencies = [
    glib_dep,
    gio_dep,
    gobject_dep,
    gee_dep,
    ggit_dep,
    git2_glib_c_dep
]

vala_files = [
    'main.vala',
    'Dist.vala'
]

executable(meson.project_name(),
    vala_files,
    dependencies: dependencies,
    c_args : ['-I'+git2_glib_dep.get_pkgconfig_variable('includedir')+'/libgit2-glib-1.0', '-DGIT_SSH=1', '-DCURL=OFF'],
    link_args : '-lm',
    install: true
)

