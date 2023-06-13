libchamplain_demos = [
  ['minimal', 'minimal.c', []],
  ['launcher', ['launcher.c', 'markers.c'], []],
  ['animated-marker', 'animated-marker.c', []],
  ['polygons', 'polygons.c', []],
  ['url-marker', 'url-marker.c', [libsoup_dep]],
  ['create_destroy_test', 'create-destroy-test.c', []],
]

foreach demo: libchamplain_demos
  demo_name = demo.get(0)
  demo_sources = demo.get(1)
  demo_deps = demo.get(2)

  executable(
    demo_name,
    demo_sources,
    install: false,
    dependencies: demo_deps + [libchamplain_dep],
  )
endforeach

subdir('icons')
subdir('memphis')

if build_gtk_widgetry
  libchamplain_gtk_demos = [
    ['minimal-gtk', 'minimal-gtk.c'],
    ['launcher-gtk', ['launcher-gtk.c', 'markers.c']],
  ]

  foreach demo: libchamplain_gtk_demos
    demo_name = demo.get(0)
    demo_sources = demo.get(1)

    executable(
      demo_name,
      demo_sources,
      install: false,
      dependencies: [
        libchamplain_dep,
        libchamplain_gtk_dep,
      ]
    )
  endforeach
endif

if generate_vapi and build_gtk_widgetry
  libchamplain_vala_demos = [
    ['launcher-vala', ['launcher-vala.vala', 'markers-vala.vala']],
  ]

  add_languages('vala')

  foreach demo: libchamplain_vala_demos
    demo_name = demo.get(0)
    demo_sources = demo.get(1)

    executable(
      demo_name,
      demo_sources,
      install: false,
      dependencies: [
        libchamplain_dep,
        libchamplain_gtk_dep,
        libchamplain_vapi,
      ]
    )
  endforeach
endif

if build_with_memphis and build_gtk_widgetry
  libchamplain_memphis_demos = [
    ['local-rendering', 'local-rendering.c'],
  ]

  foreach demo: libchamplain_memphis_demos
    demo_name = demo.get(0)
    demo_sources = demo.get(1)

    executable(
      demo_name,
      demo_sources,
      install: false,
      dependencies: [
        libchamplain_dep,
        libchamplain_gtk_dep,
      ]
    )
  endforeach
endif
