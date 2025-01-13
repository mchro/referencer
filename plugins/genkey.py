#!/usr/bin/env python
 
#  Generate Bob08 Alice99 Alice99b type keys

import referencer
from referencer import _

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk

referencer_plugin_info = {
       "author":    "John Spray",
       "version":   "1.1.2",
       "longname":  _("Generate keys from metadata"),
       "ui":
               """
               <ui>
                       <menubar name='MenuBar'>
                               <menu action='ToolsMenu'>
                                       <placeholder name ='PluginToolsActions'>
                                               <menuitem action='_plugin_genkey_genkey'/>
                                       </placeholder>
                               </menu>
                       </menubar>

                       <popup name='DocPopup'>
                               <placeholder name='PluginDocPopupActions'>
                                       <menuitem action='_plugin_genkey_genkey'/>
                               </placeholder>
                       </popup>
               </ui>
               """
       }

referencer_plugin_actions = []

action = {
    "name": "_plugin_genkey_genkey",
    "label": _("Generate Key"),
    "tooltip": _("Generate keys for the selected documents from their metadata"),
    "icon": "document-edit-symbolic",  # Updated icon for Gtk 3
    "callback": "do_genkey",
    "sensitivity": "sensitivity_genkey",
    "accelerator": "<control>g"
}
referencer_plugin_actions.append(action)

def sensitivity_genkey(library, documents):
    return len(documents) > 0

# library is always None, it's only there for future proofing
# documents is a list of referencer.document
def do_genkey(library, documents):
    empty = True
    s = ""
    assigned_keys = {}

    format = referencer.pref_get("genkey_format")
    if len(format) == 0:
        format = "%a%y"

    # Prompt the user for the key format
    dialog = Gtk.Dialog(
        title="Generate Keys",
        buttons=(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OK, Gtk.ResponseType.OK)
    )
    dialog.set_default_response(Gtk.ResponseType.OK)
    dialog.set_margin_top(10)
    dialog.set_margin_bottom(10)
    dialog.set_margin_start(10)
    dialog.set_margin_end(10)

    box = dialog.get_content_area()
    hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
    label = Gtk.Label(label="Key format:")
    entry = Gtk.Entry()
    entry.set_text(format)
    entry.set_activates_default(True)

    hbox.pack_start(label, False, False, 0)
    hbox.pack_start(entry, True, True, 0)
    box.add(hbox)

    label = Gtk.Label(label="Markers:\n\t%y = two-digit year\n\t%Y = four-digit year\n\t%a = first author's surname\n\t%t = title without spaces\n\t%w = first meaningful word of title")
    label.set_margin_top(10)
    box.add(label)

    dialog.show_all()
    response = dialog.run()

    if response != Gtk.ResponseType.OK:
        return False

    format = entry.get_text()
    dialog.destroy()

    for document in documents:
        author = document.get_field("author")
        author = author.split("and")[0].split(",")[0].split(" ")[0]
        year = document.get_field("year")
        title = document.get_field("title")
        for c in ":-[]{},+/*.?":
            title = title.replace(c, '')
        title_capitalized = "".join([w[0].upper() + w[1:] for w in title.split()])
        first_word = [w for w in title.split() if
                      w.lower() not in ('a', 'an', 'the')][0]
        title = title.replace(' ', '')

        shortYear = year
        if len(year) == 4:
            shortYear = year[2:4]

        key = format
        key = key.replace("%y", shortYear)
        key = key.replace("%Y", year)
        key = key.replace("%a", author)
        key = key.replace("%t", title)
        key = key.replace("%T", title_capitalized)
        key = key.replace("%w", first_word)

        # Make the key unique within this document set
        append = "b"
        if key in assigned_keys:
            key += append

        # Assumes <26 identical keys
        while key in assigned_keys:
            append = chr(ord(append[0]) + 1)
            key = key[0:len(key) - 1]
            key += append

        print("KEY:", key)
        assigned_keys[key] = True

        document.set_key(key)

    referencer.pref_set("genkey_format", format)

    return True
