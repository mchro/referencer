#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Referencer plugin to expand/shorten Journal names using a database file
#
# Copyright 2014-2015 Dominik Kriegner  <dominik.kriegner@gmail.com>

"""
Referencer - plugin for expanding and shortening journal names. The short and
long versions of the journal names are read from a database.

The journal name database syntax follows the one used by JabRef see
http://jabref.sourceforge.net/resources.php for details Example files with a
lot of already defined Abbreviations can be found at this web page

Briefly: lines starting with '#' are considered comments and a typical entry is

<full name> = <abbreviation>

Currently this means the names are not allowed to contain a '=' example of an
entry would be:

Physical Review B = Phys. Rev. B

The database file is read from the plugin directory were also the Python source
code is located.

after a lot of new Journal names were added the journal database can be sorted
easily using cat expj_journaldb.txt | sort > expj_journaldb_ordered.txt mv
expj_journaldb_ordered.txt expj_journaldb.txt
"""

import os
import sys
import urllib

import gtk  # for dialogs
import string  # for string handling
import difflib  # to find closest matching entry for replacement

import referencer
from referencer import _

USERPLDIR = os.path.join(os.path.expanduser("~"), ".referencer", "plugins")
DBs = []
DBs.append(os.path.join(USERPLDIR, "expj_journaldb_user.txt"))
DBs.append(os.path.join(USERPLDIR, "expj_journaldb_base.txt"))

DEFAULTMAXSUGGESTIONS = 5  # number of suggestions when no exact match is found
DEFAULTDOWNLOAD = "https://raw.githubusercontent.com/JabRef/" \
                  "reference-abbreviations/master/journals/" \
                  "journal_abbreviations_general.txt"

DEBUG = False

referencer_plugin_info = {
    "longname": _("Expand and abbreviate Journal names"),
    "author": "Dominik Kriegner",
    "version": "0.3.0",
    "ui":
        """
        <ui>
            <menubar name='MenuBar'>
                <menu action='DocMenu'>
                <placeholder name='PluginDocMenuActions'>
                    <menuitem action='_plugin_expj_expand'/>
                </placeholder>
                <placeholder name='PluginDocMenuActions'>
                    <menuitem action='_plugin_expj_shorten'/>
                </placeholder>
                </menu>
            </menubar>
            <toolbar name='ToolBar'>
            <placeholder name='PluginToolBarActions'>
                <toolitem action='_plugin_expj_expand'/>
            </placeholder>
            <placeholder name='PluginToolBarActions'>
                <toolitem action='_plugin_expj_shorten'/>
            </placeholder>
            </toolbar>
        </ui>
        """
}

referencer_plugin_actions = [
    {
        "name":        "_plugin_expj_expand",
        "label":     _("Expand Journal Name"),
        "tooltip":   _("Expand the Journal title to the long form"),
        "icon":        "expj_expand.svg",
        "callback":    "do_expand",
        "accelerator": "<control><shift>E"
    },
    {
        "name":        "_plugin_expj_shorten",
        "label":     _("Shorten Journal Name"),
        "tooltip":   _("Shorten the Journal title to the abbreviated form"),
        "icon":        "expj_shorten.svg",
        "callback":    "do_shorten",
        "accelerator": "<control><shift>C"
    }
]

# try opening the database
# when this fails the plugin will still be loaded and a hint will be shown in
# all the related dialogs
DISPHINT = True
for fname in DBs:
    try:
        with open(fname) as fh:
            DISPHINT = False
    except IOError:
        pass

# create the lists for expansion and shortening
expanded = []
contracted = []


def do_expand(library, documents):
    """
    perform shortening of Journal names for listed documents
    """
    global expanded, contracted, DISPHINT
    if len(expanded) == 0:
        # on first use of the plugin the database has to be loaded
        load_db()

    for doc in documents:
        # get current journal field
        shortv = doc.get_field('journal')
        repl = shortv
        if shortv != "":
            sv = shortv.strip().lower()
            try:
                # try direct lookup in database
                idx = map(string.lower, contracted).index(sv)
                repl = expanded[idx]
            except ValueError:
                try:  # check if journal name is already the expanded version
                    idx = map(string.lower, expanded).index(sv)
                    if idx:
                        continue
                except:
                    pass
                # no exact match was found, we will ask the user what to do
                # find 5 most likely replacements
                match = difflib.get_close_matches(shortv, contracted,
                                                  DEFAULTMAXSUGGESTIONS)
                # Prompt the user for the correct entry
                dialog = gtk.Dialog(buttons=(gtk.STOCK_CANCEL,
                                             gtk.RESPONSE_REJECT,
                                             gtk.STOCK_OK,
                                             gtk.RESPONSE_ACCEPT))
                dialog.set_has_separator(False)
                dialog.vbox.set_spacing(6)
                dialog.set_default_response(gtk.RESPONSE_ACCEPT)

                if DISPHINT:
                    text = _('No Journal name database found: consider '
                             'checking the plugin configuration dialog to '
                             'download a prepared list')
                    label = gtk.Label(text)
                    label.set_line_wrap(True)
                    image = gtk.Image()
                    image.set_from_stock(gtk.STOCK_DIALOG_INFO,
                                         gtk.ICON_SIZE_DIALOG)
                    hbox = gtk.HBox()
                    hbox.pack_start(image)
                    hbox.pack_start(label)
                    dialog.vbox.pack_start(hbox, padding=3)

                if len(match) == 0:
                    label = gtk.Label(_("No match found in database!\nEnter "
                                        "replacement entry for journal") +
                                      " '%s'" % (shortv))
                else:
                    label = gtk.Label(_("No exact match found!\n"
                                        "Choose correct entry for journal") +
                                      " '%s'" % (shortv))
                dialog.vbox.pack_start(label)
                for i in range(len(match)):
                    rbname = expanded[contracted.index(match[i])]
                    if i == 0:
                        rb = gtk.RadioButton(None, rbname)
                        rb.set_active(True)
                    else:
                        rb = gtk.RadioButton(rb, rbname)
                    dialog.vbox.pack_start(rb)

                hbox = gtk.HBox(spacing=6)
                if len(match) != 0:
                    rb = gtk.RadioButton(rb, _("Custom:"))
                else:
                    rb = gtk.Label(_("Replacement:"))

                def activate_custom(widget, rb):
                    """
                    the custom entry will be activated upon a text entry
                    """
                    rb.set_active(True)

                entry = gtk.Entry()
                entry.set_text(_("journal name"))
                entry.set_activates_default(True)
                entry.select_region(0, len(entry.get_text()))
                if len(match) != 0:
                    entry.connect("changed", activate_custom, rb)

                dialog.vbox.pack_start(hbox)
                hbox.pack_start(rb)
                hbox.pack_start(entry)
                dialog.vbox.pack_start(hbox)
                dialog.set_focus(entry)

                cb = gtk.CheckButton(label=_("Add replacement to database"))
                dialog.vbox.pack_start(cb)

                dialog.show_all()
                response = dialog.run()
                dialog.hide()

                if (response == gtk.RESPONSE_REJECT):
                    continue  # do not change anthing and continue

                # obtain users choice
                if len(match) != 0:
                    active_r = [r for r in rb.get_group() if r.get_active()][0]
                    if active_r.get_label() != _("Custom:"):
                        repl = active_r.get_label()
                    else:
                        repl = entry.get_text()
                else:
                    repl = entry.get_text()

                if cb.get_active():
                    # save the custom entry to the database
                    expanded.append(repl)
                    contracted.append(shortv)
                    save_db()

            # change the journal name
            doc.set_field('journal', repl)
            if DEBUG:
                print("expj: changed journal entry from '%s' to '%s'"
                      % (shortv, repl))

    return True


def do_shorten(library, documents):
    """
    perform shortening of Journal names for listed documents
    """
    global expanded, contracted, DISPHINT
    if len(expanded) == 0:
        # on first use of the plugin the database has to be loaded
        load_db()

    for doc in documents:
        # get current journal field
        longv = doc.get_field('journal')
        repl = longv
        if longv != "":
            lv = longv.strip().lower()
            try:  # look for exact match in database
                idx = map(string.lower, expanded).index(lv)
                repl = contracted[idx]
            except ValueError:
                try:  # check if journal name is already the shortened version
                    idx = map(string.lower, contracted).index(lv)
                    if idx:
                        continue
                except:
                    pass
                # no exact match was found, we will ask the user what to do
                # find 5 most likely replacements
                match = difflib.get_close_matches(longv, expanded,
                                                  DEFAULTMAXSUGGESTIONS)
                # Prompt the user for the correct entry
                dialog = gtk.Dialog(buttons=(gtk.STOCK_CANCEL,
                                             gtk.RESPONSE_REJECT,
                                             gtk.STOCK_OK,
                                             gtk.RESPONSE_ACCEPT))
                dialog.set_has_separator(False)
                dialog.vbox.set_spacing(6)
                dialog.set_default_response(gtk.RESPONSE_ACCEPT)

                if DISPHINT:
                    text = _('No Journal name database found: consider '
                             'checking the plugin configuration dialog to '
                             'download a prepared list')
                    label = gtk.Label(text)
                    label.set_line_wrap(True)
                    image = gtk.Image()
                    image.set_from_stock(gtk.STOCK_DIALOG_INFO,
                                         gtk.ICON_SIZE_DIALOG)
                    hbox = gtk.HBox()
                    hbox.pack_start(image)
                    hbox.pack_start(label)
                    dialog.vbox.pack_start(hbox, padding=3)

                if len(match) == 0:
                    label = gtk.Label(_("No match found in database!\nEnter "
                                        "replacement entry for journal") +
                                      " '%s'" % (longv))
                else:
                    label = gtk.Label(_("No exact match found!\n"
                                        "Choose correct entry for journal") +
                                      " '%s'" % (longv))
                dialog.vbox.pack_start(label)
                for i in range(len(match)):
                    rbname = contracted[expanded.index(match[i])]
                    if i == 0:
                        rb = gtk.RadioButton(None, rbname)
                        rb.set_active(True)
                    else:
                        rb = gtk.RadioButton(rb, rbname)
                    dialog.vbox.pack_start(rb)

                hbox = gtk.HBox(spacing=6)
                if len(match) != 0:
                    rb = gtk.RadioButton(rb, _("Custom:"))
                else:
                    rb = gtk.Label(_("Replacement:"))

                def activate_custom(widget, rb):
                    """
                    the custom entry will be activated upon a text entry
                    """
                    rb.set_active(True)

                entry = gtk.Entry()
                entry.set_text(_("journal abbreviation"))
                entry.set_activates_default(True)
                entry.select_region(0, len(entry.get_text()))
                if len(match) != 0:
                    entry.connect("changed", activate_custom, rb)

                dialog.vbox.pack_start(hbox)
                hbox.pack_start(rb)
                hbox.pack_start(entry)
                dialog.vbox.pack_start(hbox)
                dialog.set_focus(entry)

                cb = gtk.CheckButton(label=_("Add replacement to database"))
                dialog.vbox.pack_start(cb)

                dialog.show_all()
                response = dialog.run()
                dialog.hide()

                if (response == gtk.RESPONSE_REJECT):
                    continue  # do not change anthing and continue

                # obtain users choice
                if len(match) != 0:
                    active_r = [r for r in rb.get_group() if r.get_active()][0]
                    if active_r.get_label() != _("Custom:"):
                        repl = active_r.get_label()
                    else:
                        repl = entry.get_text()
                else:
                    repl = entry.get_text()

                if cb.get_active():
                    # save the custom entry to the database
                    expanded.append(longv)
                    contracted.append(repl)
                    save_db()

            # change the journal name
            doc.set_field('journal', repl)
            if DEBUG:
                print("expj: changed journal entry from '%s' to '%s'"
                      % (longv, repl))

    return True


def load_db():
    """
    Load Journal names from database file upon first use of the module
    """
    global expanded, contracted, DISPHINT
    for fname in DBs:
        try:
            with open(fname) as fh:
                for line in fh.readlines():
                    splitline = line.strip().split('=')
                    if len(splitline) == 2:
                        longv, shortv = splitline
                        expanded.append(longv.strip())
                        contracted.append(shortv.strip())
                    elif DEBUG:
                        print("expj: unparsable line in Journal name database "
                              "(%s)" % line.strip())
            DISPHINT = False  # at least one file was loaded so remove the hint
        except IOError:
            pass


def save_db():
    """
    Save current database to text file to keep changes for next
    start of Referencer
    """
    global expanded, contracted
    try:
        if not os.path.exists(os.path.dirname(DBs[0])):
            os.makedirs(os.path.dirname(DBs[0]))
        with open(DBs[0], 'a') as fh:
            newentry = expanded[-1]
            newentry += " = "
            newentry += contracted[-1]
            fh.write(newentry + '\n')
    except IOError:
        if DEBUG:
            print("expj: changes to Journal name database could not be "
                  "written! (File: %s)" % DBs[0])


def download_db(link):
    """
    Download Journal name database from the given link and save it to the
    database file, afterwards trigger a reread of all databases
    """
    global expanded, contracted

    def dlProgress(count, blockSize, totalSize):
        percent = int(count * blockSize * 100 / totalSize)
        # if one finds an easy way how to bring this info back in the GUI
        # feel free to insert your code here
        if DEBUG:
            sys.stdout.write('.')
            if percent >= 100:
                sys.stdout.write('\n')
            sys.stdout.flush()

    if DEBUG:
        print("expj: Download link '%s'" % link)
    try:
        filename, headers = urllib.urlretrieve(link, reporthook=dlProgress)
    except IOError as err:
        downloadException(err.strerror)
        return

    # check typ of download before proceeding
    if headers.type != 'text/plain':
        downloadException("Document type must be text/plain and not %s"
                          % headers.type)
        return
    # get download
    with open(filename) as fid:
        # check number of valid entries
        validlines = []
        for line in fid:
            splitline = line.strip().split('=')
            if len(splitline) == 2:
                validlines.append(line)
    if len(validlines) == 0:
        downloadException('No valid entries found in download!')
        return
    # write to base database
    if not os.path.exists(os.path.dirname(DBs[1])):
        os.makedirs(os.path.dirname(DBs[1]))
    with open(DBs[1], 'w') as fh:
        for line in validlines:
            fh.write(line)
    if DEBUG:
        print("expj: written %d entries into Journal name database!"
              % len(validlines))
    # clean old databases to trigger reload upon next usage
    expanded = []
    contracted = []


class downloadException(gtk.Dialog):
    def __init__(self, errmsg, parent=None):
        gtk.Dialog.__init__(self, "expj plugin",
                            parent,
                            gtk.DIALOG_MODAL |
                            gtk.DIALOG_DESTROY_WITH_PARENT,
                            (gtk.STOCK_OK, gtk.RESPONSE_OK))
        text = """
<b>    expj Download Error</b>

The error message is:
"%s"

Check the specified link and try again
""" % errmsg

        label = gtk.Label()
        label.set_markup(text)
        self.vbox.pack_start(label)
        self.vbox.show_all()
        self.run()
        self.destroy()


class preferencesDialog(gtk.Dialog):
    def __init__(self, parent=None):
        gtk.Dialog.__init__(self, "expj plugin configuration",
                            parent,
                            gtk.DIALOG_MODAL |
                            gtk.DIALOG_DESTROY_WITH_PARENT,
                            (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                             gtk.STOCK_OK, gtk.RESPONSE_OK))

        label = gtk.Label(_("Journal name database download link:"))
        self.dllink = gtk.Entry()
        self.dllink.set_text(DEFAULTDOWNLOAD)

        self.vbox.pack_start(label, padding=3)
        self.vbox.pack_start(self.dllink, padding=3)

        hbox = gtk.HBox()
        text = _('The above link should direct to a text file following the '
                 'syntax described on the JabRef web page where also other '
                 'possible useful Journal name database files can be found.\n'
                 '<a href="http://jabref.sourceforge.net/resources.php">'
                 'http://jabref.sourceforge.net/resources.php</a>\nBe aware '
                 'that the downloaded file will replace your current Journal '
                 'name database. Your custom entries saved in a separate file '
                 'will remain unchanged!')
        label = gtk.Label()
        label.set_markup(text)
        label.set_line_wrap(True)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_DIALOG_INFO, gtk.ICON_SIZE_DIALOG)
        hbox.pack_start(image)
        hbox.pack_start(label)
        self.vbox.pack_start(hbox, padding=3)
        self.vbox.show_all()


# Main referencer preferences function
def referencer_config():
    """
    function run by the referencer plugin configure button found in
    Edit -> Preferences
    """
    prefs = preferencesDialog()
    response = prefs.run()
    link = prefs.dllink.get_text()
    prefs.destroy()
    if response == int(gtk.RESPONSE_OK):
        download_db(link)
