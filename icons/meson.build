icons_16 = [
  'nm-device-wired.png',
  'nm-no-connection.png',
  'nm-vpn-standalone-lock.png'
]

icons_22 = [
  'nm-adhoc.png',
  'nm-device-wired.png',
  'nm-device-wwan.png',
  'nm-mb-roam.png',
  'nm-no-connection.png',
  'nm-secure-lock.png',
  'nm-signal-00.png',
  'nm-signal-100.png',
  'nm-signal-25.png',
  'nm-signal-50.png',
  'nm-signal-75.png',
  'nm-stage01-connecting01.png',
  'nm-stage01-connecting02.png',
  'nm-stage01-connecting03.png',
  'nm-stage01-connecting04.png',
  'nm-stage01-connecting05.png',
  'nm-stage01-connecting06.png',
  'nm-stage01-connecting07.png',
  'nm-stage01-connecting08.png',
  'nm-stage01-connecting09.png',
  'nm-stage01-connecting10.png',
  'nm-stage01-connecting11.png',
  'nm-stage02-connecting01.png',
  'nm-stage02-connecting02.png',
  'nm-stage02-connecting03.png',
  'nm-stage02-connecting04.png',
  'nm-stage02-connecting05.png',
  'nm-stage02-connecting06.png',
  'nm-stage02-connecting07.png',
  'nm-stage02-connecting08.png',
  'nm-stage02-connecting09.png',
  'nm-stage02-connecting10.png',
  'nm-stage02-connecting11.png',
  'nm-stage03-connecting01.png',
  'nm-stage03-connecting02.png',
  'nm-stage03-connecting03.png',
  'nm-stage03-connecting04.png',
  'nm-stage03-connecting05.png',
  'nm-stage03-connecting06.png',
  'nm-stage03-connecting07.png',
  'nm-stage03-connecting08.png',
  'nm-stage03-connecting09.png',
  'nm-stage03-connecting10.png',
  'nm-stage03-connecting11.png',
  'nm-tech-3g.png',
  'nm-tech-cdma-1x.png',
  'nm-tech-edge.png',
  'nm-tech-evdo.png',
  'nm-tech-gprs.png',
  'nm-tech-hspa.png',
  'nm-tech-lte.png',
  'nm-tech-umts.png',
  'nm-vpn-active-lock.png',
  'nm-vpn-connecting01.png',
  'nm-vpn-connecting02.png',
  'nm-vpn-connecting03.png',
  'nm-vpn-connecting04.png',
  'nm-vpn-connecting05.png',
  'nm-vpn-connecting06.png',
  'nm-vpn-connecting07.png',
  'nm-vpn-connecting08.png',
  'nm-vpn-connecting09.png',
  'nm-vpn-connecting10.png',
  'nm-vpn-connecting11.png',
  'nm-vpn-connecting12.png',
  'nm-vpn-connecting13.png',
  'nm-vpn-connecting14.png',
  'nm-wwan-tower.png'
]

icons_32 = [
  'nm-device-wired.png',
  'nm-no-connection.png'
]

icons_48 = ['nm-device-wireless.png']

icons_scalable = [
  'nm-device-wired.svg',
  'nm-no-connection.svg'
]

icons = [
  # local dir, install dir, files
  ['16', '16x16', icons_16],
  ['22', '22x22', icons_22],
  ['32', '32x32', icons_32],
  ['48', '48x48', icons_48],
  ['scalable', 'scalable', icons_scalable]
]

foreach icon: icons
  icon_dir = join_paths(nma_datadir, 'icons', 'hicolor', icon[1], 'apps')
  foreach name: icon[2]
    install_data(
      join_paths(icon[0], name),
      install_dir: icon_dir
    )
  endforeach
endforeach
