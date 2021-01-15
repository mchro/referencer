
/*
 * Referencer is released under the GNU General Public License v2
 * See the COPYING file for licensing details.
 *
 * Copyright 2007 John Spray
 * (Exceptions listed in README)
 *
 */



#include <iostream>

#include <glibmm/i18n.h>

#include "Preferences.h"


Preferences *_global_prefs;

#define CONF_PATH "/apps/referencer"
#define LIST_VIEW "list"
#define ICON_VIEW "icon"

Preferences::Preferences ()
{
	m_settings = Gio::Settings::create("apps.referencer");
	m_settings->signal_changed().connect(sigc::mem_fun(*this, &Preferences::onSettingsChange));

	xml_ = Gtk::Builder::create_from_file 
			(Utility::findDataFile ("preferences.ui"));


	xml_->get_widget ("Preferences", dialog_);

	/*
	 * Plugins
	 */

	xml_->get_widget ("PluginConfigure", configureButton_);
	configureButton_->signal_clicked().connect (
		sigc::mem_fun (*this, &Preferences::onPluginConfigure));
	xml_->get_widget ("PluginAbout", aboutButton_);
	aboutButton_->signal_clicked().connect (
		sigc::mem_fun (*this, &Preferences::onPluginAbout));

	
	auto disabledPlugins = getDisabledPlugins();
	// Iterate over all plugins
	std::list<Plugin*> plugins = _global_plugins->getPlugins();
	std::list<Plugin*>::iterator pit = plugins.begin();
	std::list<Plugin*>::iterator const pend = plugins.end();
	for (; pit != pend; pit++) {
		// All enabled unless disabled
		(*pit)->setEnabled(true);

		auto dit = disabledPlugins.begin();
		auto const dend = disabledPlugins.end();
		for (; dit != dend; ++dit) {
			if ((*pit)->getShortName() == (*dit)) {
				(*pit)->setEnabled(false);
				DEBUG (String::ucompose ("disabling plugin %1", (*pit)->getShortName()));
			}
		}
	}

	Gtk::TreeModel::ColumnRecord pluginCols;
	pluginCols.add (colPriority_);
	pluginCols.add (colPlugin_);
	pluginCols.add (colEnabled_);
	pluginCols.add (colShortName_);
	pluginCols.add (colLongName_);
	pluginStore_ = Gtk::ListStore::create (pluginCols);

	// Re-use plugins list from enable/disable stage above
	std::list<Plugin*>::iterator it = plugins.begin();
	std::list<Plugin*>::iterator const end = plugins.end();
	for (int count = 0; it != end; ++it, ++count) {
		Gtk::TreeModel::iterator item = pluginStore_->append();
		(*item)[colPriority_] = count;
		(*item)[colShortName_] = (*it)->getShortName ();
		(*item)[colLongName_] = (*it)->getLongName ();
		(*item)[colEnabled_] = (*it)->isEnabled ();
		(*item)[colPlugin_] = (*it);
	}

	xml_->get_widget ("Plugins", pluginView_);

	Gtk::CellRendererToggle *toggle = Gtk::manage (new Gtk::CellRendererToggle);
	toggle->property_activatable() = true;
	toggle->signal_toggled().connect (sigc::mem_fun (*this, &Preferences::onPluginToggled));
	Gtk::TreeViewColumn *enabled = Gtk::manage (new Gtk::TreeViewColumn (_("Enabled"), *toggle));
	enabled->add_attribute (toggle->property_active(), colEnabled_);
	pluginView_->append_column (*enabled);

	Gtk::TreeViewColumn *shortName = Gtk::manage (new Gtk::TreeViewColumn (_("Module"), colShortName_));
	pluginView_->append_column (*shortName);
	Gtk::TreeViewColumn *longName = Gtk::manage (new Gtk::TreeViewColumn (_("Description"), colLongName_));
	pluginView_->append_column (*longName);

	pluginView_->set_model (pluginStore_);
	pluginSel_ = pluginView_->get_selection ();
	pluginSel_->signal_changed().connect(
		sigc::mem_fun (*this, &Preferences::onPluginSelect));
	onPluginSelect ();


	/*
	 * End of Plugins
	 */
	ignoreChanges_ = false;
}



Preferences::~Preferences ()
{

}

void Preferences::onSettingsChange (const Glib::ustring& key)
{
	ignoreChanges_ = true;

	// Settings not in dialog
	if (key == "work-offline") {
		workofflinesignal_.emit ();
	} else if (key == "view-type") {
		uselistviewsignal_.emit ();
	} else if (key == "show-tag-pane") {
		showtagpanesignal_.emit ();
	} else if (key == "show-notes-pane") {
		shownotespanesignal_.emit ();
	} else if (key == "disabled-plugins") {
		plugindisabledsignal_.emit ();
	} else if (
	    /* keys to ignore */
	    key == "window-width"
	    || key == "window-size"
	    || key == "notes-pane-height"
	    || key == "library-filename"
	    || key == "crossref-username"
	    || key == "crossref-password"
	    || key == "list-sort"
	    ) {
		;
	} else {
		DEBUG (String::ucompose("unhandled key %1", key));
	}

	ignoreChanges_ = false;
}

void Preferences::showDialog ()
{
	dialog_->run ();
	dialog_->hide ();
}


Glib::ustring Preferences::getLibraryFilename ()
{
	return m_settings->get_string("library-filename");
}


void Preferences::setLibraryFilename (Glib::ustring const &filename)
{
	m_settings->set_string("library-filename", filename);
}


bool Preferences::getWorkOffline ()
{
	return m_settings->get_boolean("work-offline");
}


void Preferences::setWorkOffline (bool const &offline)
{
	m_settings->set_boolean("work-offline", offline);
}


sigc::signal<void>& Preferences::getWorkOfflineSignal ()
{
	return workofflinesignal_;
}

sigc::signal<void>& Preferences::getPluginDisabledSignal ()
{
	return plugindisabledsignal_;
}


bool Preferences::getUseListView ()
{
	return m_settings->get_string("view-type") == LIST_VIEW;
}


void Preferences::setUseListView (bool const &uselistview)
{
	m_settings->set_string("view-type", uselistview ? LIST_VIEW : ICON_VIEW);
}


sigc::signal<void>& Preferences::getUseListViewSignal ()
{
	return uselistviewsignal_;
}


bool Preferences::getShowTagPane ()
{
	return m_settings->get_boolean("show-tag-pane");
}


void Preferences::setShowTagPane (bool const &showtagpane)
{
	m_settings->set_boolean("show-tag-pane", showtagpane);
}


sigc::signal<void>& Preferences::getShowTagPaneSignal ()
{
	return showtagpanesignal_;
}

bool Preferences::getShowNotesPane ()
{
	return m_settings->get_boolean("show-notes-pane");
}


void Preferences::setShowNotesPane (bool const &shownotespane)
{
	m_settings->set_boolean("show-notes-pane", shownotespane);
}


sigc::signal<void>& Preferences::getShowNotesPaneSignal ()
{
	return shownotespanesignal_;
}



Glib::ustring Preferences::getCrossRefUsername ()
{
	return m_settings->get_string("crossref-username");
}


Glib::ustring Preferences::getCrossRefPassword ()
{
	return m_settings->get_string("crossref-password");
}


void Preferences::setCrossRefUsername (Glib::ustring const &username)
{
	m_settings->set_string("crossref-username", username);
}


void Preferences::setCrossRefPassword (Glib::ustring const &password)
{
	m_settings->set_string("crossref-password", password);
}


std::pair<int, int> Preferences::getWindowSize ()
{
	auto value = Glib::Variant<std::tuple<int, int>>();
	m_settings->get_value("window-size", value);

	return std::pair<int, int>(
			std::get<0>(value.get()),
			std::get<1>(value.get()));
}

void Preferences::setWindowSize (std::pair<int, int> size)
{
	Glib::VariantBase value = Glib::Variant<std::tuple<int, int>>::create(size);
	m_settings->set_value("window-size", value);
}

int Preferences::getNotesPaneHeight ()
{
	return m_settings->get_int("notes-pane-height");
}

void Preferences::setNotesPaneHeight (int height)
{
	m_settings->set_int("notes-pane-height", height);
}

std::pair<Glib::ustring, int> Preferences::getListSort ()
{

	auto value = Glib::Variant<std::tuple<Glib::ustring, int>>();
	m_settings->get_value("list-sort", value);

	return std::pair<Glib::ustring, int>(
			std::get<0>(value.get()),
			std::get<1>(value.get()));
}

void Preferences::setListSort (Glib::ustring const &columnName, int const order)
{
	auto value = Glib::Variant<std::tuple<Glib::ustring, int>>::create(
			std::make_tuple(columnName, order));
	m_settings->set_value("list-sort", value);
}

std::vector<Glib::ustring> Preferences::getDisabledPlugins() {
	auto disabledPluginsSetting = Glib::Variant<std::vector<Glib::ustring>>();
	m_settings->get_value("disabled-plugins", disabledPluginsSetting);
	return disabledPluginsSetting.get();
}

void Preferences::setDisabledPlugins (std::vector<Glib::ustring> disabledPlugins)
{
	auto value = Glib::Variant<std::vector<Glib::ustring>>::create(disabledPlugins);
	m_settings->set_value("disabled-plugins", value);
}

void Preferences::onPluginToggled (Glib::ustring const &str)
{
	Gtk::TreePath path(str);

	Gtk::TreeModel::iterator it = pluginStore_->get_iter (path);
	bool enable = !(*it)[colEnabled_];
	Plugin *plugin = (*it)[colPlugin_];
		plugin->setEnabled (enable);

	if (enable) {
		DEBUG (String::ucompose("enabling plugin %1", plugin->getShortName()));
	} else {
		DEBUG (String::ucompose("disabling plugin %1", plugin->getShortName()));
	}

	(*it)[colEnabled_] = plugin->isEnabled ();
	if (plugin->isEnabled() != enable) {
		if (plugin->hasError()) {
			TextDialog dialog (_("Plugin error"), plugin->getError());
			dialog.run ();
		}
	}

	savePluginEnabledToSettings(plugin);
}

void Preferences::savePluginEnabledToSettings(Plugin *plugin) {
	auto disable = getDisabledPlugins();
	std::vector<Glib::ustring>::iterator dit = disable.begin();
	std::vector<Glib::ustring>::iterator const dend = disable.end();
	if (plugin->isEnabled() == true) {
		// Remove from list of disabled plugins
		for (; dit != dend; ++dit) {
			if (*dit == plugin->getShortName()) {
				disable.erase(dit);
				break;
			}
		}
	} else {
		// Add to list of disabled plugins
		bool found = false;
		for (; dit != dend; ++dit)
			if (*dit == plugin->getShortName())
				found = true;
		if (!found)
			disable.push_back(plugin->getShortName());
	}
	setDisabledPlugins(disable);
}


void Preferences::disablePlugin (Plugin *plugin)
{
	plugin->setEnabled (false);
	savePluginEnabledToSettings(plugin);
}


/**
 * Update sensitivities on plugin selection changed
 */
void Preferences::onPluginSelect ()
{
	if (pluginSel_->count_selected_rows () == 0) {
		aboutButton_->set_sensitive(false);	
		configureButton_->set_sensitive(false);	
	} else {
		Gtk::ListStore::iterator it = pluginSel_->get_selected();
		Plugin *plugin = (*it)[colPlugin_];
		aboutButton_->set_sensitive(true);
		configureButton_->set_sensitive(plugin->canConfigure());
	}
}


/**
 * Display a Gtk::AboutDialog for the selected plugin
 */
void Preferences::onPluginAbout ()
{
	Gtk::ListStore::iterator it = pluginSel_->get_selected();
	Plugin *plugin = (*it)[colPlugin_];

	Gtk::AboutDialog dialog;
	dialog.set_name (plugin->getShortName());
	dialog.set_version (plugin->getVersion());
	dialog.set_comments (plugin->getLongName());
	if (!plugin->getAuthor().empty()) {
	    dialog.set_copyright ("Authors: " + plugin->getAuthor());
	}
	dialog.run ();
}


/**
 * Call the selected plugin's configuration hook if it has one
 */
void Preferences::onPluginConfigure ()
{
	Gtk::ListStore::iterator it = pluginSel_->get_selected();
	Plugin *plugin = (*it)[colPlugin_];
	plugin->doConfigure ();
}

/**
 * Store a setting on behalf of a plugin
 */
void Preferences::setPluginPref (Glib::ustring const &key, Glib::ustring const &value)
{
	m_settings->set_string("plugin-" + key, value);
}


/**
 * Retrieve a setting on behalf of a plugin
 */
Glib::ustring Preferences::getPluginPref (Glib::ustring const &key)
{
	return m_settings->get_string("plugin-" + key);
}
