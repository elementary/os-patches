include /usr/share/dpkg/default.mk

PLATFORM := UNKNOWN-PLATFORM
EFI_NAME := UNKNOWN-EFI-NAME

ifeq ($(DEB_HOST_ARCH),amd64)
PLATFORM := x86_64-efi
EFI_NAME := x64
endif

ifeq ($(DEB_HOST_ARCH),arm64)
PLATFORM := arm64-efi
EFI_NAME := aa64
endif

SIGNED := \
	grub$(EFI_NAME).efi.signed \
	gcd$(EFI_NAME).efi.signed \
	grubnet$(EFI_NAME).efi.signed

all: $(SIGNED)

$(SIGNED):
	./download-grub2

install: $(SIGNED)
	install -d $(DESTDIR)/usr/lib/grub/$(PLATFORM)-signed
	install -m0644 $(SIGNED) version \
		$(DESTDIR)/usr/lib/grub/$(PLATFORM)-signed/

clean:
	rm -f $(SIGNED) version
