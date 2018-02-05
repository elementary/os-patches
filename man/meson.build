mans = [
  'nm-applet.1',
  'nm-connection-editor.1'
]

man1_dir = join_paths(nma_mandir, 'man1')

foreach man: mans
  configure_file(
    input: man + '.in',
    output: man,
    install: true,
    install_dir: man1_dir,
    # FIXME: empty configuration
    configuration: configuration_data()
  )
endforeach
