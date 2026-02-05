#!/usr/bin/env python3

#  Generate Bob08 Alice99 Alice99b type keys

import os
import referencer
from referencer import _

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk

referencer_plugin_info = {
	"author":    "John Spray",
	"version":   "0.0",
	"longname":  "Test bibtex_to_fields",
	"ui":
		"""
		<ui>
			<popup name='DocPopup'>
				<placeholder name='PluginDocPopupActions'>
					<menuitem action='_plugin_bibtex-test'/>
				</placeholder>
			</popup>
		</ui>
		"""
	}

referencer_plugin_actions = []

action = {
	"name":"_plugin_bibtex-test",
	"label":"Print Bibtex",
	"tooltip":"Print Bibtex",
	"icon":"_stock:gtk-print",
	"callback":"do_print_bibtex",
	"sensitivity":"sensitivity_print_bibtex"
}
referencer_plugin_actions.append (action)

def sensitivity_print_bibtex (library, documents):
	if (len(documents) > 0):
		return True
	else:
	 	return False

def do_print_bibtex (library, documents):

	for doc in documents:
		print(doc.print_bibtex(False, False))

	dialog = gtk.Dialog ()
	dialog.add_buttons (gtk.STOCK_CANCEL, gtk.ResponseType.REJECT, gtk.STOCK_OK, gtk.ResponseType.ACCEPT)
	dialog.vbox.set_spacing (6)

	label = gtk.Label (label="Can has bibtex?")
	dialog.vbox.pack_start (label, True, True, 0)

	entry = gtk.Entry ()
	entry.set_text ("")
	dialog.vbox.pack_start (entry, True, True, 0)

	dialog.show_all ()
	response = dialog.run ()
	dialog.hide ()

	if (response == gtk.ResponseType.REJECT):
		return False
	else:
		bibtex_text = entry.get_text ()
		fields = referencer.bibtex_to_fields(bibtex_text)
		print(fields)
		for field in fields:
			print(field)

	return True
