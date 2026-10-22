# BANKMASTER BY KALOX — dmgbuild settings (https://dmgbuild.readthedocs.io)
# Usage (see build_dmg.sh):
#   dmgbuild -s dmg_settings.py -D pkg=... -D readme=... -D uninstall=... \
#            -D background=... -D icon=... "BANKMASTER BY KALOX" out.dmg
# dmgbuild writes the Finder layout (.DS_Store) directly, so no Finder / GUI session is needed.
import os.path

pkg = defines["pkg"]
readme = defines["readme"]
uninstall = defines["uninstall"]

files = [
    (pkg, "Install BANKMASTER.pkg"),
    (readme, "Read Me - Leia-me.txt"),
    (uninstall, "Uninstall BANKMASTER.command"),
]
symlinks = {}

format = "UDZO"          # zlib-compressed, readable on every supported macOS
filesystem = "HFS+"
size = None              # computed automatically
icon = defines.get("icon")
badge_icon = None

background = defines["background"]
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
sidebar_width = 180
window_rect = ((200, 140), (720, 460))
default_view = "icon-view"
show_icon_preview = False
include_icon_view_settings = True
include_list_view_settings = False
arrange_by = None
grid_offset = (0, 0)
grid_spacing = 100
scroll_position = (0, 0)
label_pos = "bottom"
text_size = 12
icon_size = 88

# must match the pedestals drawn in installer/assets/generate_assets.py
icon_locations = {
    "Install BANKMASTER.pkg": (200, 268),
    "Read Me - Leia-me.txt": (430, 268),
    "Uninstall BANKMASTER.command": (580, 268),
}
