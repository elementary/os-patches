po_dir = meson.current_source_dir()
project_version = meson.project_version()

i18n.gettext(meson.project_name(),
  preset: 'glib',
  args: [
    '--keyword=Title',
    '--keyword=SubTitle',
    '--from-code=UTF-8',
    f'--package-version=@project_version@',
    '--copyright-holder=Plymouth contributors',
    '--msgid-bugs-address=https://gitlab.freedesktop.org/plymouth/plymouth/issues',
  ],
)
