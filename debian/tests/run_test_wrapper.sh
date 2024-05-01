#!/bin/sh

KERNEL_VER=
for kver in $(dpkg-query -W -f '${Package}\n' 'linux-headers-*' | sed s/linux-headers-//)
do
	if [ -d "/lib/modules/$kver/build" ]
	then
		KERNEL_VER=$kver
		break
	fi
done

export KERNEL_VER

# On Ubuntu, signing is supported on UEFI secureboot amd64 & arm64
# only for now.
case `dpkg --print-architecture` in
	amd64|arm64)
		bash ./run_test.sh
		;;
	*)
		bash ./run_test.sh --no-signing-tool
		;;
esac
