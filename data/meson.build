if get_option('enable-backend')
    conf = configuration_data()

    if get_option('demo-agent')
        conf.set('demo_agent', 'geoclue-demo-agent;')
    else
        conf.set('demo_agent', '')
    endif

    conf_dir = join_paths(sysconfdir, 'geoclue')
    configure_file(output: 'geoclue.conf',
                   input: 'geoclue.conf.in',
                   configuration: conf,
                   install_dir: conf_dir)

    conf = configuration_data()
    conf.set('libexecdir', libexecdir)
    conf.set('dbus_srv_user', get_option('dbus-srv-user'))
    conf.set('sysconfdir', sysconfdir)

    service_dir = join_paths(datadir, 'dbus-1', 'system-services')
    configure_file(output: 'org.freedesktop.GeoClue2.service',
                   input: 'org.freedesktop.GeoClue2.service.in',
                   configuration: conf,
                   install_dir: service_dir)

    man_file = configure_file(output: 'geoclue.5',
                              input: 'geoclue.5.in',
                              configuration: conf)

    install_man(man_file)

    # DBus Service policy file
    dbus_service_dir = get_option('dbus-sys-dir')
    if dbus_service_dir == ''
        dbus_service_dir = join_paths(sysconfdir, 'dbus-1', 'system.d')
    endif
    configure_file(output: 'org.freedesktop.GeoClue2.conf',
                   input: 'org.freedesktop.GeoClue2.conf.in',
                   configuration: conf,
                   install_dir: dbus_service_dir)
    configure_file(output: 'org.freedesktop.GeoClue2.Agent.conf',
                   input: 'org.freedesktop.GeoClue2.Agent.conf.in',
                   configuration: conf,
                   install_dir: dbus_service_dir)

    systemd_unit_dir = get_option('systemd-system-unit-dir')
    if systemd_unit_dir == ''
        dep = dependency('systemd', required: false)
        if dep.found()
            systemd_unit_dir = dep.get_pkgconfig_variable('systemdsystemunitdir')
        endif
    endif

    if systemd_unit_dir != ''
        configure_file(output: 'geoclue.service',
                       input: 'geoclue.service.in',
                       configuration: conf,
                       install_dir: systemd_unit_dir)
    endif
endif
