#!/usr/bin/gjs

//
// This example shows how to use the mobile providers database code from
// within Javascript through gobject-introspection.
//

const NMGtk = imports.gi.NMGtk;

var mpd = new NMGtk.MobileProvidersDatabase();
try {
    mpd.init(null);

    let provider = mpd.lookup_3gpp_mcc_mnc('21403');
    if (provider)
        log ('Provider: ' + provider.get_name());
    else
        log ('Unknown provider');
} catch (e) {
    logError (e, 'Error while reading database');
}


