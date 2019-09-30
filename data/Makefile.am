servicedir=@datarootdir@/dbus-1/services
service_DATA=org.ayatana.bamf.service

EXTRA_DIST=org.ayatana.bamf.service.in

org.ayatana.bamf.service: org.ayatana.bamf.service.in
	$(AM_V_GEN)sed "s,@PKGLIBEXECDIR@,$(pkglibexecdir)," $< > $@

CLEANFILES = org.ayatana.bamf.service
