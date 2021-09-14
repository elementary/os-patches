all:
	mkdir -p build
	# Download unsigned & signed shim
	./download-signed shim current shim signed

	# Copy unsigned shim
	cp $(SHIM_VERSION)/$(SHIM_BASE) build/

	# Create detached signature from externally signed shim
	sbattach --detach external-$(EFI_ARCH_LOWER).p7c external-shim$(EFI_ARCH_LOWER).efi

	# Attach external signature to unsigned & signed shims
	sbattach --attach external-$(EFI_ARCH_LOWER).p7c $(SHIM_VERSION)/$(SHIM_BASE)
	sbattach --attach external-$(EFI_ARCH_LOWER).p7c $(SHIM_VERSION)/$(SHIM_BASE).signed

	# Copy external & dualsigned shims
	cp $(SHIM_VERSION)/$(SHIM_BASE) build/$(SHIM_BASE).signed
	cp $(SHIM_VERSION)/$(SHIM_BASE).signed build/$(SHIM_BASE).dualsigned

	# Move archive signed fb*.efi & mm*.efi
	mv $(SHIM_VERSION)/$(FB_BASE).signed build/$(FB_BASE)
	mv $(SHIM_VERSION)/$(MM_BASE).signed build/$(MM_BASE)

	# Generate BOOT.CSV
	echo "$(SHIM_BASE),ubuntu,,This is the boot entry for ubuntu" | iconv -t UCS-2LE > build/BOOT$(EFI_ARCH).CSV

check:
	# Verify all signatures
	sbverify --verbose --cert external.pem build/$(SHIM_BASE).signed
	sbverify --verbose --cert external.pem build/$(SHIM_BASE).dualsigned
	sbverify --verbose --cert $(SHIM_VERSION)/control/uefi.crt build/$(SHIM_BASE).dualsigned
	sbverify --verbose --cert $(SHIM_VERSION)/control/uefi.crt build/$(FB_BASE)
	sbverify --verbose --cert $(SHIM_VERSION)/control/uefi.crt build/$(MM_BASE)
	# verify sbattach binary output matches the externally-signed binary
	cmp external-shim$(EFI_ARCH_LOWER).efi build/$(SHIM_BASE).signed

clean:
	rm -rf build $(SHIM_VERSION) external-$(EFI_ARCH_LOWER).p7c
