xsltproc = find_program('xsltproc')

man_pages = {
  'plymouth1.xml': 'plymouth.1',
  'plymouth.xml': 'plymouth.8',
  'plymouthd.xml': 'plymouthd.8',
  'plymouth-set-default-theme.xml': 'plymouth-set-default-theme.1',
}

foreach man_xml_input, man_output : man_pages
  custom_target(man_output,
    input: man_xml_input,
    output: man_output,
    command: [
      xsltproc,
      '--nonet',
      '--stringparam', 'man.output.quietly', '1',
      '--stringparam', 'funcsynopsis.style', 'ansi',
      '--stringparam', 'man.th.extra1.suppress', '1',
      '--stringparam', 'man.authors.section.enabled', '0',
      '--stringparam', 'man.copyright.section.enabled', '0',
      '-o', '@OUTPUT@',
      'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
      '@INPUT@',
    ],
    install: true,
    install_dir: get_option('mandir') / 'man' + man_output.substring(-1),
  )
endforeach
