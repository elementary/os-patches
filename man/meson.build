
xsltproc = find_program('xsltproc', required : true)

xml_man_src = [
  'user-dirs.conf.xml',
  'user-dirs.defaults.xml',
  'user-dirs.dirs.xml',
  'xdg-user-dir.xml',
  'xdg-user-dirs-update.xml'
]

# Generate man pages from XML sources
foreach xml_file : xml_man_src
  basename = xml_file.split('.xml')[0]

  # Most are section 5 (config files), except the executables which are section 1
  if basename.startswith('xdg-user-dir')
    section = '1'
  else
    section = '5'
  endif

  man_file = basename + '.' + section

  custom_target(man_file,
    input: xml_file,
    output: man_file,
    command: [
      xsltproc,
      '--nonet',
      '--stringparam', 'man.output.quietly', '1',
      '--stringparam', 'funcsynopsis.style', 'ansi',
      '-o', '@OUTPUT@',
      'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
      '@INPUT@'
    ],
    build_by_default: true,
    install: true,
    install_dir: get_option('mandir') / ('man' + section)
  )
endforeach
