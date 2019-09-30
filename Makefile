SIGNED := \
	grubx64.efi.signed \
	gcdx64.efi.signed \
	grubnetx64.efi.signed

all: $(SIGNED)

$(SIGNED):
	./download-grub2

install: $(SIGNED)
	install -d $(DESTDIR)/usr/lib/grub/x86_64-efi-signed
	install -m0644 $(SIGNED) version \
		$(DESTDIR)/usr/lib/grub/x86_64-efi-signed/

clean:
	rm -f $(SIGNED) version
