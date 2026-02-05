
/*
 * Referencer is released under the GNU General Public License v2
 * See the COPYING file for licensing details.
 *
 * Copyright 2007 John Spray
 * (Exceptions listed in README)
 *
 */



#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <utility>

#include <gtkmm.h>

#include "Utility.h"
#include "PluginManager.h"

class Preferences {
private:
	Glib::RefPtr<Gtk::Builder> xml_;

	/*
	 * Plugins
	 */
	Gtk::TreeView *pluginView_;
	Gtk::Button *moveUpButton_;
	Gtk::Button *moveDownButton_;
	Gtk::Button *configureButton_;
	Gtk::Button *aboutButton_;
	Glib::RefPtr<Gtk::ListStore> pluginStore_;
	Glib::RefPtr<Gtk::TreeView::Selection> pluginSel_;

	Gtk::TreeModelColumn<unsigned int> colPriority_;
	Gtk::TreeModelColumn<Plugin*> colPlugin_;
	Gtk::TreeModelColumn<Glib::ustring> colShortName_;
	Gtk::TreeModelColumn<Glib::ustring> colLongName_;
	Gtk::TreeModelColumn<bool> colEnabled_;

	void onPluginToggled (Glib::ustring const &str);
	void onPluginSelect ();
	void onPluginAbout ();
	void onPluginConfigure ();

	std::vector<Glib::ustring> getDisabledPlugins();
	void setDisabledPlugins (std::vector<Glib::ustring> disabledPlugins);
	void savePluginEnabledToSettings(Plugin *plugin);
	public:
	void disablePlugin (Plugin *plugin);

	void setPluginPref (Glib::ustring const &key, Glib::ustring const &value);
	Glib::ustring getPluginPref (Glib::ustring const &key);

	/*
	 * End of Plugins
	 */
	

	/*
	 * Conf for crossref plugin
	 */
public:
	Glib::ustring getCrossRefUsername ();
	Glib::ustring getCrossRefPassword ();
	void setCrossRefUsername (Glib::ustring const &username);
	void setCrossRefPassword (Glib::ustring const &password);

	/*
	 * End of conf for crossref plugin
	 */

	/*
	 * List view options
	 */
public:
	std::pair<Glib::ustring, int> getListSort ();
	void setListSort (Glib::ustring const &columnName, int const order);
	/*
	 * End of list view options
	 */

	/*
	 * Uncategorised
	 */
private:
	Gtk::Dialog *dialog_;

	Gtk::Entry *proxyhostentry_;
	Gtk::SpinButton *proxyportspin_;
	Gtk::Entry *proxyusernameentry_;
	Gtk::Entry *proxypasswordentry_;
	Gtk::CheckButton *useproxycheck_;
	Gtk::CheckButton *useauthcheck_;

	void onWorkOfflineToggled ();
	void onProxyChanged ();
	void updateSensitivity ();

	void onSettingsChange (const Glib::ustring& key);

	sigc::signal<void> workofflinesignal_;
	sigc::signal<void> uselistviewsignal_;
	sigc::signal<void> showtagpanesignal_;
	sigc::signal<void> shownotespanesignal_;
	sigc::signal<void> plugindisabledsignal_;

	bool ignoreChanges_;

	Glib::RefPtr<Gio::Settings> m_settings;

	Glib::KeyFile pluginPrefsKeyFile_;
	std::string pluginPrefsPath_;
	void loadPluginPrefs ();
	void savePluginPrefs ();

public:
	Preferences ();
	~Preferences ();
	void showDialog ();

	Glib::ustring getLibraryFilename ();
	void setLibraryFilename (Glib::ustring const &filename);

	bool getWorkOffline ();
	void setWorkOffline (bool const &offline);
	sigc::signal<void>& getWorkOfflineSignal ();

	sigc::signal<void>& getPluginDisabledSignal ();

	bool getUseListView ();
	void setUseListView (bool const &uselistview);
	sigc::signal<void>& getUseListViewSignal ();

	bool getShowTagPane ();
	void setShowTagPane (bool const &showtagpane);
	sigc::signal<void>& getShowTagPaneSignal ();
	
	bool getShowNotesPane ();
	void setShowNotesPane (bool const &shownotespane);
	sigc::signal<void>& getShowNotesPaneSignal ();

	typedef std::pair<Glib::ustring, Glib::ustring> StringPair;

	std::pair<int, int> getWindowSize ();
	void setWindowSize (std::pair<int, int> size);
	
	int getNotesPaneHeight ();
	void setNotesPaneHeight (int height);
};

extern Preferences *_global_prefs;


#endif
