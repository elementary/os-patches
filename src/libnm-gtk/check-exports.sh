#!/bin/sh

LC_ALL=C
export LC_ALL

stat=0
so="$1"
def="$2"
PATTERN="_ANCHOR_"

TMPFILE="$(mktemp .nm-check-exports.XXXXXX)"


get_syms() {
    ${NM:-nm} "$1" |
    sed -n 's/^[[:xdigit:]]\+ [DT] //p' |
    sort
}

get_1_0_0_syms() {
	cat <<SYMBOLS
nma_country_info_get_country_code
nma_country_info_get_country_name
nma_country_info_get_providers
nma_country_info_get_type
nma_country_info_ref
nma_country_info_unref
nma_mobile_access_method_get_3gpp_apn
nma_mobile_access_method_get_dns
nma_mobile_access_method_get_family
nma_mobile_access_method_get_gateway
nma_mobile_access_method_get_name
nma_mobile_access_method_get_password
nma_mobile_access_method_get_type
nma_mobile_access_method_get_username
nma_mobile_access_method_ref
nma_mobile_access_method_unref
nma_mobile_provider_get_3gpp_mcc_mnc
nma_mobile_provider_get_cdma_sid
nma_mobile_provider_get_methods
nma_mobile_provider_get_name
nma_mobile_provider_get_type
nma_mobile_provider_ref
nma_mobile_providers_database_dump
nma_mobile_providers_database_get_countries
nma_mobile_providers_database_get_type
nma_mobile_providers_database_lookup_3gpp_mcc_mnc
nma_mobile_providers_database_lookup_cdma_sid
nma_mobile_providers_database_lookup_country
nma_mobile_providers_database_new
nma_mobile_providers_database_new_finish
nma_mobile_providers_database_new_sync
nma_mobile_providers_split_3gpp_mcc_mnc
nma_mobile_provider_unref
nma_mobile_wizard_destroy
nma_mobile_wizard_new
nma_mobile_wizard_present
nma_utils_disambiguate_device_names
nma_utils_get_connection_device_name
nma_utils_get_device_description
nma_utils_get_device_generic_type_name
nma_utils_get_device_product
nma_utils_get_device_type_name
nma_utils_get_device_vendor
nma_vpn_password_dialog_focus_password
nma_vpn_password_dialog_focus_password_secondary
nma_vpn_password_dialog_get_password
nma_vpn_password_dialog_get_password_secondary
nma_vpn_password_dialog_get_type
nma_vpn_password_dialog_new
nma_vpn_password_dialog_run_and_block
nma_vpn_password_dialog_set_password
nma_vpn_password_dialog_set_password_label
nma_vpn_password_dialog_set_password_secondary
nma_vpn_password_dialog_set_password_secondary_label
nma_vpn_password_dialog_set_show_password
nma_vpn_password_dialog_set_show_password_secondary
nma_wifi_dialog_get_connection
nma_wifi_dialog_get_nag_ignored
nma_wifi_dialog_get_type
nma_wifi_dialog_nag_user
nma_wifi_dialog_new
nma_wifi_dialog_new_for_create
nma_wifi_dialog_new_for_hidden
nma_wifi_dialog_new_for_other
nma_wifi_dialog_set_nag_ignored
nma_wireless_dialog_get_connection
nma_wireless_dialog_get_type
nma_wireless_dialog_new
nma_wireless_dialog_new_for_create
nma_wireless_dialog_new_for_other
SYMBOLS
	}

get_syms_from_def() {
    (sed -n 's/^\t\(\([_a-zA-Z0-9]\+\)\|#\s*\([_a-zA-Z0-9]\+@@\?[_a-zA-Z0-9]\+\)\);$/\2\3/p' "$1";
    get_1_0_0_syms)  |
    sort
}

anchor() {
    sed "s/.*/$PATTERN\0$PATTERN/"
}

unanchor() {
    sed "s/^$PATTERN\(.*\)$PATTERN\$/\1/"
}


get_syms "$so" | anchor > "$TMPFILE"
WRONG="$(get_syms_from_def "$def" | anchor | grep -F -f - "$TMPFILE" -v)"
RESULT=$?
if [ $RESULT -eq 0 ]; then
    stat=1
    echo ">>library \"$so\" exports symbols that are not in linker script \"$def\":"
    echo "$WRONG" | unanchor | nl
fi

get_syms_from_def "$def" | anchor > "$TMPFILE"
WRONG="$(get_syms "$so" | anchor | grep -F -f - "$TMPFILE" -v)"
RESULT=$?
if [ $RESULT -eq 0 ]; then
    stat=1
    echo ">>linker script \"$def\" contains symbols that are not exported by library \"$so\":"
    echo "$WRONG" | unanchor | nl
fi

rm -rf "$TMPFILE"
exit $stat

