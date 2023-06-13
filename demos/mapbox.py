#!/usr/bin/env python

# To run this example, you need to set the GI_TYPELIB_PATH environment
# variable to point to the gir directory:
#
# export GI_TYPELIB_PATH=$GI_TYPELIB_PATH:/usr/local/lib/girepository-1.0/

import gi
gi.require_version('Champlain', '0.12')
gi.require_version('GtkChamplain', '0.12')
gi.require_version('GtkClutter', '1.0')
from gi.repository import GtkClutter
from gi.repository import Gtk, Champlain, GtkChamplain

ACCESS_TOKEN = "PUT YOUR ACCESS TOKEN HERE!!!"

CACHE_SIZE = 100000000  # size of cache stored on disk
MEMORY_CACHE_SIZE = 100 # in-memory cache size (tiles stored in memory)
MIN_ZOOM = 0
MAX_ZOOM = 19
TILE_SIZE = 256
LICENSE_TEXT = ""
LICENSE_URI = "https://www.mapbox.com/tos/"


def create_cached_source():
    factory = Champlain.MapSourceFactory.dup_default()

    tile_source = Champlain.NetworkTileSource.new_full(
        "mapbox",
        "mapbox",
        LICENSE_TEXT,
        LICENSE_URI,
        MIN_ZOOM,
        MAX_ZOOM,
        TILE_SIZE,
        Champlain.MapProjection.MERCATOR,
        "https://a.tiles.mapbox.com/v4/mapbox.streets/#Z#/#X#/#Y#.png?access_token=" + ACCESS_TOKEN,
        Champlain.ImageRenderer())

    tile_size = tile_source.get_tile_size()

    error_source = factory.create_error_source(tile_size)
    file_cache = Champlain.FileCache.new_full(CACHE_SIZE, None, Champlain.ImageRenderer())
    memory_cache = Champlain.MemoryCache.new_full(MEMORY_CACHE_SIZE, Champlain.ImageRenderer())

    source_chain = Champlain.MapSourceChain()
    # tile is retrieved in this order:
    # memory_cache -> file_cache -> tile_source -> error_source
    # the first source that contains the tile returns it
    source_chain.push(error_source)
    source_chain.push(tile_source)
    source_chain.push(file_cache)
    source_chain.push(memory_cache)

    return source_chain


GtkClutter.init([])

window = Gtk.Window(type=Gtk.WindowType.TOPLEVEL)
window.connect("destroy", Gtk.main_quit)

widget = GtkChamplain.Embed()
widget.set_size_request(640, 480)

view = widget.get_view()

view.set_map_source(create_cached_source())

window.add(widget)
window.show_all()

Gtk.main()
