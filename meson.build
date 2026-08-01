project(
  'xdg-user-dirs-gtk', 'c',
        version : '0.16',
        license : 'GPL-2.0-or-later',
  meson_version : '>= 1.0.0'
)

bindir = join_paths (get_option ('prefix'), get_option ('bindir'))
localedir = join_paths (get_option ('prefix'), get_option ('localedir'))
sysconfdir = join_paths (get_option ('prefix'), get_option ('sysconfdir'))
datadir = join_paths (get_option ('prefix'), get_option ('datadir'))
applicationsdir = join_paths (datadir, 'applications')
autostartdir = join_paths (sysconfdir, 'xdg', 'autostart')

find_program('xdg-user-dirs-update', required: true)

executable ('xdg-user-dirs-gtk-update',
            [ 'parse.c',
              'update.c' ],
            dependencies: dependency('gtk+-3.0'),
            c_args: [ '-DGETTEXT_PACKAGE="xdg-user-dirs-gtk"',
                      '-DGLIBLOCALEDIR="@0@"'.format (localedir) ],
            install: true )

# Integration with modern session startup
systemd = dependency('systemd', required: false)
systemd_user_unit_dir = systemd.get_variable(
    'systemd_user_unit_dir',
    pkgconfig_define: ['prefix', get_option('prefix')],
    default_value: get_option('prefix') / 'lib' / 'systemd' / 'user'
)
configure_file(
    input: 'user-dirs-update-gtk.service.in',
    output: 'user-dirs-update-gtk.service',
    configuration: {
        'bindir': bindir,
    },
    install: true,
    install_dir: systemd_user_unit_dir,
)

# Integration with legacy session startup
i18n = import('i18n')
i18n.merge_file (input: 'user-dirs-update-gtk.desktop.in',
                 output: 'user-dirs-update-gtk.desktop',
                 install: true,
                 install_dir: autostartdir,
                 po_dir: 'po',
                 type: 'desktop')

# data: Also install desktop file into datadir
# At least on GLib-based systems, autostart folders are not searched
# for installed .desktop files. So in order for GNOME to pick up the
# correct name and icon, install another copy into the datadir's
# application directory.
#
# Mirrors commit: fe37c50488efe8ef9605df239d3e33537cf46332
meson.add_install_script('meson_custom_install_desktop_file.sh', applicationsdir, autostartdir, skip_if_destdir: false)

subdir('po')
