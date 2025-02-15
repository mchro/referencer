
/*
 * Referencer is released under the GNU General Public License v2
 * See the COPYING file for licensing details.
 *
 * Copyright 2007 John Spray
 * (Exceptions listed in README)
 *
 */

#include <iostream>

#include <gtk/gtk.h>
#include <gtkmm.h>
#include <glibmm/i18n.h>
#include <giomm/file.h>
#include <giomm/fileinfo.h>
#include <giomm/error.h>

#include "Document.h"
#include "DocumentList.h"
#include "Library.h"
#include "Linker.h"
#include "Preferences.h"
#include "RefWindow.h"
#include "Transfer.h"
#include "ucompose.hpp"
#include "Utility.h"
#include "TagList.h"
#include "EntryMultiCompletion.h"
#include "ustring.h"
#include "DocumentCellRenderer.h"

#include "DocumentView.h"

static const Glib::ustring defaultSortColumn = "title";


void DocumentView::doEditTagsDialog(Document *doc) {
	Gtk::Dialog dialog (_("Edit tags"), *(win_.window_), true);
	/* XXX, do more clever handling of tag separation. maybe copy+modify EntryMultiCompletion class */

	Gtk::Box *vbox = dialog.get_vbox ();

	Gtk::HBox hbox;
	hbox.set_spacing (12);
	vbox->pack_start (hbox, true, true, 0);

	Gtk::Label label (_("Tags:"), false);
	hbox.pack_start (label, false, false, 0);

	std::list<Glib::ustring> items;
	Glib::ustring str_current_tags;
	TagList* taglist = lib_.getTagList();
	TagList::TagMap allTags = taglist->getTags();
	TagList::TagMap::iterator tagIter = allTags.begin();
	TagList::TagMap::iterator const tagEnd = allTags.end();
	for (; tagIter != tagEnd; ++tagIter) {
		items.push_back(tagIter->second.name_ + ",");

		if (doc->hasTag(tagIter->second.uid_)) {
			str_current_tags += tagIter->second.name_ + ", ";
		}

		/*Gtk::ListStore::iterator row = model_->append();
		(*row)[nameColumn_] = tagIter->second.name_;
		(*row)[uidColumn_] = tagIter->second.uid_;
		(*row)[selectedColumn_] = selections_[tagIter->second.uid_];*/
	}


	Glib::RefPtr<EntryMultiCompletion> completion = EntryMultiCompletion::create(items);
	
	Gtk::Entry entry;
	entry.set_text(str_current_tags);
	entry.set_completion(completion);
	entry.set_activates_default (true);
	entry.set_size_request(400, -1);
	hbox.pack_start (entry, true, true, 0);

	dialog.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button (Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
	dialog.set_default_response (Gtk::RESPONSE_ACCEPT);

	dialog.show_all ();
	vbox->set_border_width (12);

	if (dialog.run () == Gtk::RESPONSE_ACCEPT) {
		/* mangle tags back to tag uid's*/
		doc->clearTags();
		std::vector<Glib::ustring> new_tags = Glib::Util::split(entry.get_text(), ",");

		std::vector<Glib::ustring>::iterator new_tagIter = new_tags.begin();
		std::vector<Glib::ustring>::iterator const new_tagEnd = new_tags.end();
		for (; new_tagIter != new_tagEnd; ++new_tagIter) {
			Glib::Util::trim(*new_tagIter);
			if (*new_tagIter != "") {
				int taguid = taglist->getTagUid(*new_tagIter);
				if (taguid > 0) {
					doc->setTag(taguid);
				} else {
					/* XXX: Tag not found. Ask user to add? */
				}
			}
		}


	}
}


DocumentView::~DocumentView ()
{
}


/* [bert] This is a special little comparison function for titles. The
 * idea is to remove leading articles ("a", "an", or "the") from the
 * title before performing the comparison. This is relatively easy in
 * English, but might be harder (or inappropriate) in other languages.
 */
int DocumentView::sortTitles (
	const Gtk::TreeModel::iterator& a,
	const Gtk::TreeModel::iterator& b)
{
  // This callback should return
  //  * -1 if a compares before b
  //  *  0 if they compare equal
  //  *  1 if a compares after b

  Gtk::TreeModelColumn<Glib::ustring> col;

  /* It really seems like there should be an easier way to get at the
   * data to be sorted on.
   */
  col = columns_.find("title")->second.modelColumn;
  Glib::ustring a_title = Utility::removeLeadingArticle((*a)[col]);
  Glib::ustring b_title = Utility::removeLeadingArticle((*b)[col]);

  return a_title.compare(b_title);
}

DocumentView::DocumentView (
	RefWindow &refwin,
	Library &lib,
	bool const uselistview)
	
 : win_ (refwin), lib_(lib)
{
	hoverdoc_ = NULL;
	ignoreSelectionChanged_ = false;

	/*
	 * Pack a vbox inside a frame inside ourself
	 */
	Gtk::VBox *vbox = Gtk::manage(new Gtk::VBox);
	Gtk::Frame *iconsframe = new Gtk::Frame ();
	iconsframe->add (*vbox);
	pack_start (*iconsframe, true, true, 0);

	/*
	 * Model common to icon and list views
	 */
	Gtk::TreeModel::ColumnRecord doccols;

	doccols.add(docpointercol_);
	doccols.add(doccaptioncol_);
	doccols.add(docthumbnailcol_);
	doccols.add(doctooltipcol_);
	doccols.add(dockeycol_);
	doccols.add(doctitlecol_);
	doccols.add(docauthorscol_);
	doccols.add(docyearcol_);
	doccols.add (docvisiblecol_);

	docstore_ = Gtk::ListStore::create(doccols);
	docstorefilter_ = Gtk::TreeModelFilter::create (docstore_);
	docstorefilter_->set_visible_column (docvisiblecol_);
	docstoresort_ = Gtk::TreeModelSort::create (docstorefilter_);
	docstoresort_->signal_sort_column_changed ().connect(
		sigc::mem_fun (*this, &DocumentView::onSortColumnChanged));



	/*
	 * Icon View
	 */
	Gtk::IconView *icons = Gtk::manage(new Gtk::IconView(
				static_cast<Glib::RefPtr<Gtk::TreeModel>>(docstoresort_)
				));
	//icons->set_markup_column (doccaptioncol_);
	//icons->set_pixbuf_column (docthumbnailcol_);

	GtkCellLayout* cell_layout = GTK_CELL_LAYOUT(icons->gobj());
	/* FIXME: these cellrenderers never get freed */
	DocumentCellRenderer *doccell = new DocumentCellRenderer (this);

	gtk_cell_layout_pack_start (cell_layout, GTK_CELL_RENDERER(doccell->gobj()), false);

	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icons->gobj()),
		GTK_CELL_RENDERER(doccell->gobj()),
		"document", 0,
		NULL);

	Gtk::CellRendererText *textcell = new Gtk::CellRendererText ();
	textcell->property_width_chars() = 32;
	textcell->property_wrap_mode() = Pango::WRAP_WORD_CHAR;
	gtk_cell_layout_pack_start (cell_layout, GTK_CELL_RENDERER (textcell->gobj()), true);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icons->gobj()),
		GTK_CELL_RENDERER (textcell->gobj()),
		"markup", 1,
		NULL);
	
	icons->add_events (Gdk::ALL_EVENTS_MASK);
	// Nasty, gtkmm doesn't have a binding for passing the column object
	icons->set_tooltip_column (3);
	icons->signal_item_activated().connect (
		sigc::mem_fun (*this, &DocumentView::docActivated));


	icons->signal_button_press_event().connect(
		sigc::mem_fun (*this, &DocumentView::docClicked), false);

	icons->signal_selection_changed().connect(
		sigc::mem_fun (*this, &DocumentView::docSelectionChanged));

	icons->set_selection_mode (Gtk::SELECTION_MULTIPLE);
	icons->set_columns (-1);

	icons->set_item_orientation (Gtk::ORIENTATION_HORIZONTAL);
	icons->set_item_width (70);

	std::vector<Gtk::TargetEntry> dragtypes;
	Gtk::TargetEntry target;
	target.set_info (0);
	target.set_target ("text/uri-list");
	dragtypes.push_back (target);
	target.set_target ("text/x-moz-url-data");
	dragtypes.push_back (target);

	icons->drag_dest_set (
		dragtypes,
		Gtk::DEST_DEFAULT_ALL,
		Gdk::ACTION_COPY | Gdk::ACTION_MOVE | Gdk::ACTION_LINK);

	icons->signal_drag_data_received ().connect (
		sigc::mem_fun (*this, &DocumentView::onIconsDragData));
	

	icons->set_events (Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK);
	icons->signal_motion_notify_event ().connect_notify (
		sigc::mem_fun (*this, &DocumentView::onDocMouseMotion));

	docsiconview_ = icons;

	Gtk::ScrolledWindow *iconsscroll = Gtk::manage(new Gtk::ScrolledWindow());
	iconsscroll->add(*icons);
	iconsscroll->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	vbox->pack_start(*iconsscroll, true, true, 0);

	docsiconscroll_ = iconsscroll;

	/*
	 * End of icon view stuff
	 */

	/*
	 * List view stuff
	 */
	Gtk::TreeView *table = Gtk::manage (new Gtk::TreeView(docstoresort_));
	table->set_enable_search (true);
	table->set_search_column (1);
	table->set_rules_hint (true);

	table->drag_dest_set (
		dragtypes,
		Gtk::DEST_DEFAULT_ALL,
		Gdk::ACTION_COPY | Gdk::ACTION_MOVE | Gdk::ACTION_LINK);

	table->signal_drag_data_received ().connect (
		sigc::mem_fun (*this, &DocumentView::onIconsDragData));

	table->signal_row_activated ().connect (
		sigc::mem_fun (*this, &DocumentView::docListActivated));

	table->signal_button_press_event().connect(
		sigc::mem_fun (*this, &DocumentView::docClicked), false);

	docslistselection_ = table->get_selection ();
	docslistselection_->set_mode (Gtk::SELECTION_MULTIPLE);
	docslistselection_->signal_changed ().connect (
		sigc::mem_fun (*this, &DocumentView::docSelectionChanged));

	docslistview_ = table;

	Gtk::ScrolledWindow *tablescroll = Gtk::manage(new Gtk::ScrolledWindow());
	tablescroll->add(*table);
	tablescroll->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	vbox->pack_start(*tablescroll, true, true, 0);

	docslistscroll_ = tablescroll;

	populateColumns ();

	/*
	 * End of list view stuff
	 */
	
	setUseListView (uselistview);
	

	/*
	 * Search box
	 */
	Gtk::Entry *searchentry = new Gtk::SearchEntry();
	searchentry->signal_changed ().connect (
		sigc::mem_fun (*this, &DocumentView::onSearchChanged));
	
	searchentry_ = searchentry;
	/*
	 * End of search box
	 */


	/*
	 * Sorting
	 * (This isn't done at model construction because it needs to be after populateColumns)
	 */

	/* [bert] - Insert the specialized sort function for the title
	 */
	Gtk::TreeModelColumn<Glib::ustring> col = columns_.find("title")->second.modelColumn;
	docstoresort_->set_sort_func(col, sigc::mem_fun(*this, &DocumentView::sortTitles));

	std::pair<Glib::ustring, int> sortInfo = _global_prefs->getListSort ();
	std::map<Glib::ustring, Column>::iterator columnIter = columns_.find(sortInfo.first);
	if (columnIter != columns_.end()) {
		DEBUG ("Initialising sort column to %1", sortInfo.first);
		docstoresort_->set_sort_column (
				columnIter->second.modelColumn,
				(Gtk::SortType)sortInfo.second);
	} else {
		DEBUG ("Initialising sort column to default, '%1'", defaultSortColumn);
		docstoresort_->set_sort_column (
				(columns_.find(defaultSortColumn))->second.modelColumn,
				(Gtk::SortType)sortInfo.second);
	}
	/*
	 * End of sorting
	 */
}


void DocumentView::setUseListView (bool const &list)
{
	if (list != uselistview_) {
		uselistview_ = list;

        if (uselistview_) {
            docsiconscroll_->hide ();
            docslistscroll_->show ();
            docslistview_->grab_focus ();
        } else {
            docslistscroll_->hide ();
            docsiconscroll_->show ();
            docsiconview_->grab_focus ();
        }

        docSelectionChanged ();
	}
}


void DocumentView::onDocMouseMotion (GdkEventMotion* event)
{
	int x = (int)event->x;
	int y = (int)event->y;

	Gtk::TreeModel::Path path = docsiconview_->get_path_at_pos (x, y);

	Document *doc = NULL;
	if (!path.empty()) {
		Gtk::ListStore::iterator it = docstoresort_->get_iter (path);
		doc = (*it)[docpointercol_];
	}


	if (doc != hoverdoc_) {
		Document *oldHoverDoc = hoverdoc_;
			hoverdoc_ = doc;
		if (oldHoverDoc)
		    redraw (oldHoverDoc);
		if (hoverdoc_)
		    redraw (hoverdoc_);
	}

}



void DocumentView::onIconsDragData (
	const Glib::RefPtr <Gdk::DragContext> &context,
	int n1, int n2, const Gtk::SelectionData &sel, guint n3, guint n4)
{
	DEBUG ("Type '" + sel.get_data_type () + "'");

	std::vector<Glib::ustring> files;

	typedef std::vector <Glib::ustring> urilist;
	urilist uris;

	if (sel.get_data_type () == "text/uri-list") {
		uris = sel.get_uris ();
	} else if (sel.get_data_type () == "text/x-moz-url-data") {
		uris.push_back (Utility::mozUrlSelectionToUTF8(sel));
	}

	urilist::iterator it = uris.begin ();
	urilist::iterator const end = uris.end ();
	for (; it != end; ++it) {
		bool is_dir = false;
		Glib::RefPtr<Gio::File> uri = Gio::File::create_for_uri (*it);

		{
			// It's a local file, see if it's a directory
			Glib::RefPtr<Gio::FileInfo> info;
			try {
				info = uri->query_info ();
			} catch (const Gio::Error& ex) {
				Utility::exceptionDialog (&ex,
					String::ucompose (
						_("Getting info for file '%1'"),
						uri->get_uri ()));
				return;
			}
			is_dir = info->get_file_type () == Gio::FILE_TYPE_DIRECTORY;

			if (is_dir) {
				std::vector<Glib::ustring> morefiles = Utility::recurseFolder (*it);
				std::vector<Glib::ustring>::iterator it2;
				for (it2 = morefiles.begin(); it2 != morefiles.end(); ++it2) {
					files.push_back (*it2);
				}
			} else {
				files.push_back (*it);
			}
		}
	}

	win_.addDocFiles (files);
}


std::vector<Document*> DocumentView::getSelectedDocs ()
{
	std::vector<Document*> docpointers;

	if (uselistview_) {
		auto paths =
			docslistselection_->get_selected_rows ();

		auto it = paths.begin ();
		auto const end = paths.end ();
		for (; it != end; it++) {
			Gtk::TreePath sortPath = (*it);
			Gtk::ListStore::iterator iter = docstoresort_->get_iter(sortPath);
			docpointers.push_back((*iter)[docpointercol_]);
		}
	} else {
		auto paths =
			docsiconview_->get_selected_items ();

		auto it = paths.begin ();
		auto const end = paths.end ();
		for (; it != end; it++) {
			Gtk::TreePath sortPath = (*it);
			Gtk::ListStore::iterator iter = docstoresort_->get_iter(sortPath);
			Document *docptr = (*iter)[docpointercol_];

			if (!docptr) {
				/* This is really bad and we will probably crash, but 
				 * at least we will know why by looking at stdout.  
				 * Propagate the error because this should never 
				 * happen and I don't want it to go unnoticed */
				DEBUG ("Unset document pointer!");
			}

			docpointers.push_back(docptr);
		}
	}

	return docpointers;
}


Document *DocumentView::getSelectedDoc ()
{
	if (uselistview_) {
		auto paths =
			docslistselection_->get_selected_rows ();

		if (paths.size() != 1) {
			DEBUG ("Warning: DocumentView::getSelectedDoc: size != 1");
			return NULL;
		}

		Gtk::TreePath path = (*paths.begin ());
		Gtk::ListStore::iterator iter = docstoresort_->get_iter (path);
		return (*iter)[docpointercol_];

	} else {
		auto paths =
			docsiconview_->get_selected_items ();

		if (paths.size() != 1) {
			DEBUG ("Warning: DocumentView::getSelectedDoc: size != 1");
			return NULL;
		}

		Gtk::TreePath path = (*paths.begin ());
		Gtk::ListStore::iterator iter = docstoresort_->get_iter (path);
		return (*iter)[docpointercol_];
	}
}


int DocumentView::getSelectedDocCount ()
{
	if (uselistview_) {
		return docslistselection_->get_selected_rows().size ();
	} else {
		return docsiconview_->get_selected_items().size ();
	}
}

std::vector<Document*> DocumentView::getVisibleDocs ()
{
	std::vector<Document*> docpointers;

	Gtk::ListStore::iterator it = docstoresort_->children().begin();
	Gtk::ListStore::iterator const end = docstoresort_->children().end();
	for (; it != end; it++) {
		Document *docptr = (*it)[docpointercol_];
		docpointers.push_back(docptr);
	}
	return docpointers;
}

int DocumentView::getVisibleDocCount ()
{
	return docstoresort_->children().size();
}

void DocumentView::invokeLinker (Linker *linker)
{
	std::vector<Document*> docs = getSelectedDocs ();

	std::vector<Document*>::iterator it = docs.begin ();
	std::vector<Document*>::iterator const end = docs.end ();
	for (; it != end; it++) {
		if (linker->canLink (*it))
			linker->doLink(*it);
	}
}

bool DocumentView::docClicked (GdkEventButton* event)
{
	if((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
		/*
		 * Select what's under the pointer if it isn't already
		 * selected
		 */
		if (uselistview_) {
			Gtk::TreeModel::Path clickedpath;
			Gtk::TreeViewColumn *clickedcol;
			int cellx;
			int celly;

			bool const gotpath = docslistview_->get_path_at_pos (
				(int)event->x, (int)event->y,
				clickedpath, clickedcol,
				cellx, celly);

			if (gotpath && !docslistselection_->is_selected (clickedpath)) {
				docslistselection_->unselect_all ();
				docslistselection_->select (clickedpath);
			}
		} else {
			Gtk::TreeModel::Path clickedpath =
				docsiconview_->get_path_at_pos ((int)event->x, (int)event->y);

			if (!clickedpath.empty()
			    && !docsiconview_->path_is_selected (clickedpath)) {
				docsiconview_->unselect_all ();
				docsiconview_->select_path (clickedpath);
			}
		}
		
		popupContextMenu (event);

		return true;
	} else if ((event->type == GDK_BUTTON_PRESS) && (event->button == 2)) {
		// Middle click pasting
		win_.onPasteBibtex (GDK_SELECTION_PRIMARY);
		return true;
	} else {
		return false;
	}
}


void DocumentView::popupContextMenu (GdkEventButton* event)
{
	/*
	 * Linkers are local to here for now, in the future they might
	 * be plugin-extensible and thus moved elsewhere
	 */
	static std::vector<Linker*> linkers;
	static DoiLinker doi;
	static ArxivLinker arxiv;
	static UrlLinker url;
	static PubmedLinker pubmed;
	static GoogleLinker google;

	/*
	 * Initialise linkers
	 */
	static Glib::RefPtr<Gdk::Pixbuf> linkerIcon;
	if (linkers.size() == 0) {
		linkers.push_back(&doi);
		linkers.push_back(&arxiv);
		linkers.push_back(&url);
		linkers.push_back(&pubmed);
		linkers.push_back(&google);

		std::vector<Linker*>::iterator it = linkers.begin ();
		std::vector<Linker*>::iterator const end = linkers.end ();
		for (; it != end; ++it)
			(*it)->createUI (&win_, this);
	}

	/*
	 * Get the popup menu widget
	 */
	Gtk::Menu *popupmenu =
		(Gtk::Menu*)win_.uimanager_->get_widget("/DocPopup");

	/* Work out which linkers are applicable */
	std::vector<Document *> docs = getSelectedDocs ();
		
	std::vector<Linker*>::iterator it = linkers.begin();
	std::vector<Linker*>::iterator const end = linkers.end();
	for (; it != end; ++it) {
		Linker *linker = (*it);
		Glib::RefPtr<Gtk::Action> action =
		    win_.actiongroup_->get_action (Glib::ustring("linker_") + linker->getName());

		bool enable = false;
		std::vector<Document*>::iterator docIt = docs.begin ();
		std::vector<Document*>::iterator const docEnd = docs.end ();
		for (; docIt != docEnd; ++docIt) {
			if (linker->canLink(*docIt))
				enable = true;
		}

		action->set_visible(enable);
	}

	/*
	 * Display the menu
	 */
	popupmenu->popup_at_pointer (reinterpret_cast<GdkEvent*>(event));
}


DocumentView::SubSet DocumentView::selectedDocsHaveTag (int uid)
{
	bool alltrue = true;
	bool allfalse = true;

	std::vector<Document*> docs = getSelectedDocs ();

	std::vector<Document*>::iterator it = docs.begin ();
	std::vector<Document*>::iterator const end = docs.end ();
	for (; it != end; it++) {
		if ((*it)->hasTag(uid)) {
			allfalse = false;
		} else {
			alltrue = false;
		}
	}

	if (allfalse == true)
		return NONE;
	else if (alltrue == true)
		return ALL;
	else if (alltrue == false && allfalse == false)
		return SOME;
	else
		return NONE;
}


DocumentView::Capabilities DocumentView::getDocSelectionCapabilities ()
{
	Capabilities result;

	std::vector<Document*> docs = getSelectedDocs ();

	std::vector<Document*>::iterator it = docs.begin ();
	std::vector<Document*>::iterator const end = docs.end ();

	bool const offline = _global_prefs->getWorkOffline();

	for (; it != end; it++) {
		// Allow web linking even when offline, since it might
		// just be us that's offline, not the web browser
		if ((*it)->canGetMetadata() && !offline)
			result.getmetadata = true;

		if (!(*it)->getFileName().empty())
			result.open = true;
	}

	return result;
}


/*
 * Selection changed, not our problem, let the parent deal.
 */
void DocumentView::docSelectionChanged ()
{
	if (!ignoreSelectionChanged_)
		selectionchangedsignal_.emit ();
}


/**
 * End-point of
 *  - double clicking in treeview
 *  - double clicking in iconview
 *  - activation in cellrenderer
 */
void DocumentView::docActivate (Document *doc)
{
	// The methods we're calling should fail out safely and quietly
	// if the number of docs selected != 1
	if (!doc->getFileName ().empty ()) {
		win_.onOpenDoc ();
	} else {
		win_.onDocProperties ();
	}
}


/*
 * User double clicked on a document
 */
void DocumentView::docActivated (const Gtk::TreeModel::Path& path)
{
	Gtk::TreePath filterPath = docstoresort_->convert_path_to_child_path (path);
	Gtk::TreePath realPath = docstorefilter_->convert_path_to_child_path (filterPath);


	Gtk::ListStore::iterator it = docstore_->get_iter (realPath);
	docActivate ((*it)[docpointercol_]);
}


/*
 * Populate a row in docstore_ from a Document
 */
void DocumentView::loadRow (
	Gtk::TreeModel::iterator item,
	Document * const doc)
{
	(*item)[docpointercol_] = doc;
	(*item)[dockeycol_] = doc->getKey();
	(*item)[docthumbnailcol_] = doc->getThumbnail();
	(*item)[doctitlecol_] = doc->getBibData().getTitle ();
	(*item)[docauthorscol_] = doc->getBibData().getAuthors ();
	(*item)[docyearcol_] = doc->getBibData().getYear ();

	Glib::ustring tooltipText =
		String::ucompose(
				"<b>%1</b>\n",
				Glib::Markup::escape_text(doc->getKey()));

	typedef std::map <Glib::ustring, Glib::ustring> StringMap;
	StringMap fields = doc->getFields ();
	StringMap::iterator fieldIter = fields.begin();
	StringMap::iterator const fieldEnd = fields.end();
	for (; fieldIter != fieldEnd; ++fieldIter) {
		tooltipText += "\n";
		tooltipText += Glib::Markup::escape_text (fieldIter->first);
		tooltipText += ": ";
		tooltipText += Glib::Markup::escape_text (fieldIter->second.substr(0,64));
		if (fieldIter->second.size() > 64)
			tooltipText += "...";
	}

	(*item)[doctooltipcol_] = tooltipText;
	(*item)[docvisiblecol_] = isVisible (doc);

	Glib::ustring title = Utility::wrap (doc->getField ("title"), 35, 1, false);
	Glib::ustring authors =	Utility::firstAuthor(doc->getField("author"));
	Glib::ustring etal = "";
	authors = Utility::strip (authors, "{");
	authors = Utility::strip (authors, "}");
	authors = Utility::wrap (authors, 33, 1, false);
	Glib::ustring::size_type n = doc->getField("author").find (" and");
	if (n != Glib::ustring::npos)
		etal = " et al.";
	

	Glib::ustring key = doc->getKey();
	Glib::ustring year = doc->getField("year");
	Glib::ustring align = "\n";


	if (year.empty() and authors.empty())
		align = "";
	if (year == "")
		authors = String::ucompose ("%1%2 ", authors, etal);
	else
		authors = String::ucompose (" %1%2 ", authors, etal);
	if (title.empty())
		title = " ";
	if (authors.empty())
		authors = "";
	if (key.empty())
		key = " ";
	if (year.empty())
		year = "";

	(*item)[doccaptioncol_] = String::ucompose (
		// Translators: this is the format for the document captions
		_("<small>%1</small>\n<b>%2</b>\n%3<i>%4</i>%5"),
		Glib::Markup::escape_text (key),
		Glib::Markup::escape_text (title),
		Glib::Markup::escape_text (year),
		Glib::Markup::escape_text (authors),
		align);
}

/*
 * Return whether a document matches searches and tag filters
 */

bool DocumentView::isVisible (Document * const doc)
{
	Glib::ustring const searchtext = searchentry_->get_text ();
	bool const search = !searchtext.empty ();

	bool visible = true;
	for (std::vector<int>::iterator tagit = win_.filtertags_.begin();
	     tagit != win_.filtertags_.end(); ++tagit) {
		if (!(*tagit == ALL_TAGS_UID
		    || (*tagit == NO_TAGS_UID && doc->getTags().empty())
		    || doc->hasTag(*tagit))) {
		    	// A tag is selected that we do not match
			visible = false;
			break;
		}
	}

	if (search && visible) {
		if (!doc->matchesSearch (searchtext))
			visible = false;
	}
	
	/*
	 * TODO iterate over taggerUris 
	 */
	 
	return visible;
}

/*
 * Update the visibility of all rows
 *
 * This needs to be called:
 *  - when global conditions for visibility
 *     change: when the tag filter is changed, when search text is 
 *     changed
 *
 *  Things like adding and updating docs don't need to call this as well
 *
 */
void DocumentView::updateVisible ()
{
	ignoreSelectionChanged_ = true;
	Gtk::TreeModel::iterator item = docstore_->children().begin();
	Gtk::TreeModel::iterator const end = docstore_->children().end();
	for (; item != end; ++item) {
		Document * const doc = (*item)[docpointercol_];
		(*item)[docvisiblecol_] = isVisible (doc);
	}
	ignoreSelectionChanged_ = false;

	//Scroll to the currently selected document, if any
	Gtk::TreePath selpath;
	if (uselistview_) {
		auto paths =
			docslistselection_->get_selected_rows ();
		if (paths.size () > 0) {
			selpath = (*paths.begin());
			docslistview_->scroll_to_row (selpath);
		}
	} else {
		auto paths =
			docsiconview_->get_selected_items ();
		if (paths.size () > 0) {
			selpath = (*paths.begin());
			docsiconview_->scroll_to_path (selpath, true, 0.5, 0.0);
		}
	}

	docSelectionChanged ();
}


/*
 * Optimisation: O(N)
 *
 *  - Refresh document's row in docstore_ from Document.
 *  - Update visibility of document's row
 */
void DocumentView::updateDoc (Document * const doc)
{
	Gtk::TreeModel::iterator item = docstore_->children().begin();
	Gtk::TreeModel::iterator const end = docstore_->children().end();
	for (; item != end; ++item) {
		if ((*item)[docpointercol_] == doc) {
			loadRow (item, doc);
			return;
		}
	}
	
	DEBUG ("DocumentView::updateDoc: Warning: doc not found");
}


/*
 * Remove the row with docpointercol_ == doc from docstore_
 */
void DocumentView::removeDoc (Document * const doc)
{
	bool found = false;

	ignoreSelectionChanged_ = true;
	Gtk::TreeModel::iterator it = docstore_->children().begin();
	Gtk::TreeModel::iterator const end = docstore_->children().end();
	for (; it != end; ++it) {
		if ((*it)[docpointercol_] == doc) {
			docstore_->erase (it);
			found = true;
			break;
		}
	}
	ignoreSelectionChanged_ = false;
	docSelectionChanged ();
	
	if (!found)
		DEBUG ("DocumentView::removeDoc: Warning: doc not found");
}

/*
 * Append a row to docstore_ and load data from Document
 */
void DocumentView::addDoc (Document * doc, bool userTriggered)
{
	doc->setView(this);

	Gtk::TreeModel::iterator item = docstore_->append();
	loadRow (item, doc);
  
	if (userTriggered) {
		Gtk::TreeModel::Path path =
			docstoresort_->get_path (
				docstoresort_->convert_child_iter_to_iter (
					docstorefilter_->convert_child_iter_to_iter (item)));

		docslistview_->scroll_to_row (path);
		docslistselection_->unselect_all ();
		docslistselection_->select (path);

		docsiconview_->unselect_all ();
		docsiconview_->scroll_to_path (path, false, 0.0, 0.0);
		docsiconview_->select_path (path);
	}
}


/*
 * Please, please populate tags etc before calling this with 
 * a tag-related handler connected to the selectionchanged
 * signal
 */
void DocumentView::populateDocStore ()
{
	//DEBUG ("RefWindow::populateDocStore >>");
	ignoreSelectionChanged_ = true;

	/* XXX not the only one any more! */
	// This is our notification that something about the documentlist
	// has changed, including its length, so update dependent sensitivities:
	win_.actiongroup_->get_action("ExportBibtex")
		->set_sensitive (lib_.getDocList()->size() > 0);
	// Save initial selection
	Gtk::TreePath initialpath;
	if (uselistview_) {
		auto paths =
			docslistselection_->get_selected_rows ();
		if (paths.size () > 0)
			initialpath = (*paths.begin());
	} else {
		auto paths =
			docsiconview_->get_selected_items ();
		if (paths.size () > 0)
			initialpath = (*paths.begin());
	}

	docstore_->clear ();

	// Populate from library_->doclist_
	DocumentList::Container& docvec = lib_.getDocList()->getDocs();
	DocumentList::Container::iterator docit = docvec.begin();
	DocumentList::Container::iterator const docend = docvec.end();
	for (; docit != docend; ++docit) {
		addDoc (&(*docit), false);
	}

	// Restore initial selection
	if (uselistview_) {
		docslistselection_->select (initialpath);
	} else {
		docsiconview_->select_path (initialpath);
	}

	docsiconview_->unselect_all ();
	docslistselection_->unselect_all ();
	ignoreSelectionChanged_ = false;
	docSelectionChanged ();
}


/*
 * Clear out all documents
 */
void DocumentView::clear ()
{
	docstore_->clear ();
}


void DocumentView::onSearchChanged ()
{
	updateVisible ();
}


void DocumentView::onColumnEdited (
	const Glib::ustring& pathStr, 
	const Glib::ustring& enteredText,
	const Glib::ustring &columnName)
{
	Gtk::TreePath sortPath (pathStr);
	Gtk::TreePath filterPath = docstoresort_->convert_path_to_child_path (sortPath);
	Gtk::TreePath realPath = docstorefilter_->convert_path_to_child_path (filterPath);

	Gtk::TreeModel::iterator iter = docstore_->get_iter (realPath);
	Gtk::TreeModelColumn<Glib::ustring> col = columns_.find(columnName)->second.modelColumn;
	if ((*iter)[col] != enteredText) {
		Document *doc = (*iter)[docpointercol_];

		Glib::ustring newText = enteredText;

		if (columnName.lowercase() == "key") {
			Glib::ustring sanitizedKey = lib_.getDocList()->sanitizedKey (newText);
			if (sanitizedKey != newText)
				newText = Document::keyReplaceDialogInvalidChars(newText, sanitizedKey);

			Glib::ustring unique = lib_.getDocList()->uniqueKey (newText, doc);
			if (unique != newText)
				newText = Document::keyReplaceDialogNotUnique (newText, unique);
		}

		(*iter)[col] = newText;
		doc->setField (columnName, newText);
		win_.setDirty (true);
	}
}


void DocumentView::addColumn (
	Glib::ustring const &name,
	Glib::ustring const &caption,
	Gtk::TreeModelColumn<Glib::ustring> &modelCol,
	bool const expand,
	bool const ellipsize)
{
	Column column(modelCol, caption);
	std::pair<Glib::ustring, Column> pair (name, column);
	columns_.insert (pair);

	/*
	 * Create treeview column
	 */
	Gtk::CellRendererText *cell;
	Gtk::TreeViewColumn *col;

	// Er, we're actually passing this as reference, is this the right way
	// to create it?  Will the treeview actually copy it?
	col = Gtk::manage (new Gtk::TreeViewColumn (caption, modelCol));
	col->set_resizable (true);
	col->set_expand (expand);
	col->set_sort_column (modelCol);
	cell = (Gtk::CellRendererText *) col->get_first_cell();
	if (ellipsize)
		cell->property_ellipsize () = Pango::ELLIPSIZE_END;
	cell->property_editable () = true;
	cell->signal_edited ().connect (
		sigc::bind (
			sigc::mem_fun (*this, &DocumentView::onColumnEdited),
			name)
		);
	docslistview_->append_column (*col);

	/*
	 * Create gtkuimanager action
	 */

	/* Unique UI action identifier */
	Glib::ustring const actionName = String::ucompose ("sort_%1", name);
	/* Translation note: this completes the sentence started by 
	 * the "Arrange Items" string, which is the parent menu 
	 * item.  If this does not make sense in your locale, translate 
	 * "Arrange Items" as something like "Sort" and translate this 
	 * as just "%1" */
	Glib::ustring const actionCaption = String::ucompose (_("By %1"), caption);

	SortAction action;
	action.name = name;
	action.action = Gtk::RadioAction::create (sortUIGroup_, actionName, actionCaption);
	action.action->signal_toggled().connect(
			sigc::bind(
				sigc::mem_fun (*this, &DocumentView::sortActionToggled),
				action));

	/* Merge it into the UI */
	Glib::ustring ui = 
		"<ui>"
		"<menubar name='MenuBar'>"
		"<menu action='ViewMenu' name='ViewMenu'>"
		"<menu action='ViewMenuSort' name='ViewMenuSort'>"
		"  <placeholder name='ViewMenuSortItems'>"
		"    <menuitem action='";
	ui += actionName;
	ui += "'/>"
		"  </placeholder>"
		"</menu>"
		"</menu>"
		"</menubar>"
		"</ui>";

	win_.actiongroup_->add(action.action);
	try {
		action.merge = win_.uimanager_->add_ui_from_string (ui);
	} catch (Glib::Error& err) {
		DEBUG (ui);
		DEBUG ("Merge error: %1", err.what());
	}

	sortUI_[name] = action;
}


void DocumentView::sortActionToggled (SortAction const &action)
{
	if (action.action->get_active()) {
		docstoresort_->set_sort_column (
				(columns_.find(action.name))->second.modelColumn,
				Gtk::SORT_ASCENDING);

		_global_prefs->setListSort (action.name, 0);
	}
}


void DocumentView::onSortColumnChanged ()
{
	Gtk::SortType order;
	int column;
	docstoresort_->get_sort_column_id (column, order);

	Glib::ustring columnName;

	std::map<Glib::ustring, Column>::iterator columnIter = columns_.begin();
	std::map<Glib::ustring, Column>::iterator columnEnd = columns_.end();
	for (; columnIter != columnEnd; ++columnIter) {
		if ((*columnIter).second.modelColumn.index() == column) {
			columnName = (*columnIter).first;
		}
	}

	if (!columnName.empty()) {
		SortActionMap::iterator action = sortUI_.find(columnName);
		if (action != sortUI_.end()) {
			DEBUG ("Activated action for column name %1", columnName);
			action->second.action->set_active(true);
		} else {
			DEBUG ("Failed to find UI action for column name %1", columnName);
		}
		_global_prefs->setListSort (columnName, order);
	} else {
		DEBUG ("Failed to resolve column id %1 to a name", column);
	}
}


void DocumentView::populateColumns ()
{
	addColumn ("key", _("Key"), dockeycol_, false, false);
	addColumn ("title", _("Title"), doctitlecol_, true, true);
	addColumn ("author", _("Author"), docauthorscol_, false, true);
	addColumn ("year", _("Year"), docyearcol_, false, false);
}


void DocumentView::select (Document *document)
{
    /* Look up the path to this document */
    Gtk::TreeModel::iterator docIter = docstoresort_->children().begin();
    Gtk::TreeModel::iterator const docEnd = docstoresort_->children().end();
    for (; docIter != docEnd; ++docIter) {
        if ((*docIter)[docpointercol_] == document)
            break;
    }

    if (docIter == docEnd) {
        DEBUG ("DocumentView::select: warning: document %1 not found", document);
    }

    Gtk::TreeModel::Path path = docstoresort_->get_path (docIter);
    
    /* Select the path */
	if (uselistview_) {
		docslistselection_->unselect_all ();
		docslistselection_->select (path);
	} else {
		docsiconview_->unselect_all ();
		docsiconview_->select_path (path);
	}
}


void DocumentView::redraw (Document *document)
{
	/* Even if we bother poking a particular document IconView
	 * seems to redraw the whole kaboodle */
#if 0
	/* Look up the path to this document */
	Gtk::TreeModel::iterator docIter = docstoresort_->children().begin();
	Gtk::TreeModel::iterator const docEnd = docstoresort_->children().end();
	for (; docIter != docEnd; ++docIter) {
		if ((*docIter)[docpointercol_] == document)
			break;
	}

	if (docIter == docEnd) {
		DEBUG ("DocumentView::select: warning: document "
			<< document << " not found");
	}

	Gtk::TreeModel::Path path = docstoresort_->get_path (docIter);

	docstoresort_->row_changed (path, docIter);

#else
	docsiconview_->get_window()->invalidate_rect (Gdk::Rectangle (0, 0, 1000, 1000),true);
#endif
}
