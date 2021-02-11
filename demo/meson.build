desktop_dir = join_paths(datadir, 'applications')
demo_dir = join_paths(libexecdir, 'geoclue-2.0', 'demos')

i18n = import('i18n')
desktop_conf = configuration_data()
desktop_conf.set('libexecdir', libexecdir)

if get_option('libgeoclue')
    include_dirs = [ configinc,
                     libgeoclue_public_api_inc,
                     include_directories('../libgeoclue') ]

    executable('where-am-i',
               'where-am-i.c',
               include_directories: include_dirs,
               dependencies: [base_deps, libgeoclue_dep],
               install: true,
               install_dir: demo_dir)
    desktop_in = configure_file(output: 'geoclue-where-am-i.desktop.in',
                                input: 'geoclue-where-am-i.desktop.in.in',
                                configuration: desktop_conf)
    i18n.merge_file(output: 'geoclue-where-am-i.desktop',
                    input: desktop_in,
                    type: 'desktop',
                    po_dir: '../po',
                    install: true,
                    install_dir: desktop_dir)
endif

if get_option('demo-agent')
    include_dirs = [ configinc,
                     libgeoclue_public_api_inc ]

    sources = [ 'gclue-service-agent.h',
                'gclue-service-agent.c',
                'agent.c',
                 geoclue_agent_sources ]
    deps = base_deps + [ dependency('libnotify') ]
    executable('agent',
               sources,
               include_directories: include_dirs,
               dependencies: deps,
               install: true,
               install_dir: demo_dir)

    desktop_in = configure_file(output: 'geoclue-demo-agent.desktop.in',
                                input: 'geoclue-demo-agent.desktop.in.in',
                                configuration: desktop_conf)
    desktop_file = i18n.merge_file(output: 'geoclue-demo-agent.desktop',
                                   input: desktop_in,
                                   type: 'desktop',
                                   po_dir: '../po',
                                   install: true,
                                   install_dir: desktop_dir)

    # Also install in the autostart directory.
    autostart_dir = join_paths(sysconfdir, 'xdg', 'autostart')
    meson.add_install_script('install-file.py',
                             desktop_file.full_path(),
                             autostart_dir)
endif
