
i18n = import('i18n')

gettext_domain = 'xdg-user-dirs'
i18n.gettext(meson.project_name(),
  preset: 'glib',
  args: [
        '--default-domain=' + gettext_domain,
        '--from-code=UTF-8',
        '-i', '-F', '-c', '--no-wrap',
        '--package-name=' + gettext_domain,
    ]
)
