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
	current/grub$(EFI_NAME).efi.signed \
	current/gcd$(EFI_NAME).efi.signed \
	current/grubnet$(EFI_NAME).efi.signed

all: $(SIGNED)

$(SIGNED):
	./download-signed grub2-common current grub2 uefi

install: $(SIGNED)
	install -d $(DESTDIR)/usr/lib/grub/$(PLATFORM)-signed
	install -m0644 $(SIGNED) current/version \
		$(DESTDIR)/usr/lib/grub/$(PLATFORM)-signed/

clean:
	rm -rf current/
