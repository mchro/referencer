/*
 * Referencer is released under the GNU General Public License v2
 * See the COPYING file for licensing details.
 *
 * Copyright 2007 John Spray
 * (Exceptions listed in README)
 *
 */

#include <iostream>
#include <sstream>

#include <glibmm/i18n.h>
#include <libxml/xmlwriter.h>
#include "ucompose.hpp"
#include <poppler.h>

#include "config.h"

#include "BibUtils.h"
#include "DocumentView.h"
#include "Library.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "TagList.h"
#include "ThumbnailGenerator.h"
#include "Utility.h"

#include "Document.h"
#include "Library.h"

const Glib::ustring Document::defaultKey_ = _("Unnamed");
Glib::RefPtr<Gdk::Pixbuf> Document::loadingthumb_;


Document::~Document ()
{
	ThumbnailGenerator::instance().deregisterRequest (this);
}

Document::Document (Document const &x)
{
	view_ = NULL;
	*this = x;
	setupThumbnail ();
}

Document::Document (Glib::ustring const &filename)
{
	view_ = NULL;
	setFileName (filename);
}


Document::Document ()
{
	view_ = NULL;
	// Pick up the default thumbnail
	setupThumbnail ();
}

Document::Document (
	Glib::ustring const &filename,
	Glib::ustring const &relfilename,
	Glib::ustring const &notes,
	Glib::ustring const &key,
	std::vector<int> const &tagUids,
	BibData const &bib)
{
	view_ = NULL;
	setFileName (filename);
	setNotes (notes);
	key_ = key;
	tagUids_ = tagUids;
	bib_ = bib;
	relfilename_ = relfilename;
}

Document::Document(xmlNodePtr docNode) 
{
    readXML(docNode);
}

Glib::ustring Document::keyReplaceDialogNotUnique (
	Glib::ustring const &original,
	Glib::ustring const &replacement)
{
	return keyReplaceDialog(original, replacement,
		_("The chosen key conflicts with an "
			"existing one.  Replace '%1' with '%2'?"));
}

Glib::ustring Document::keyReplaceDialogInvalidChars (
	Glib::ustring const &original,
	Glib::ustring const &replacement)
{
	return keyReplaceDialog(original, replacement,
		_("The chosen key contained invalid characters."
			"  Replace '%1' with '%2'?"));
}

Glib::ustring Document::keyReplaceDialog (
	Glib::ustring const &original,
	Glib::ustring const &replacement,
	const char *message_text)
{
	Glib::ustring message = String::ucompose (
		"<big><b>%1</b></big>\n\n%2",
		_("Key naming conflict"),
		String::ucompose (
			message_text,
			original, replacement));

	Gtk::MessageDialog dialog (message, true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);
	Gtk::Button *button;

	button = dialog.add_button (_("_Ignore"), Gtk::RESPONSE_CANCEL);
	Gtk::Image *noImage = Gtk::manage (new Gtk::Image (Gtk::Stock::NO, Gtk::ICON_SIZE_BUTTON));
	button->set_image (*noImage);

	button = dialog.add_button (_("_Replace"), Gtk::RESPONSE_ACCEPT);
	Gtk::Image *yesImage = Gtk::manage (new Gtk::Image (Gtk::Stock::YES, Gtk::ICON_SIZE_BUTTON));
	button->set_image (*yesImage);
	dialog.set_default_response (Gtk::RESPONSE_ACCEPT);

	if (dialog.run () == Gtk::RESPONSE_ACCEPT)
		return replacement;
	else
		return original;
}


Glib::ustring Document::generateKey ()
{
	// Ideally Chambers06
	// If not then pap104
	// If not then Unnamed-5
	Glib::ustring name;

	Glib::ustring::size_type const maxlen = 14;

	if (!bib_.getAuthors().empty ()) {
		Glib::ustring year = bib_.getYear ();
		if (year.size() == 4)
			year = year.substr (2,3);

		Glib::ustring authors = bib_.getAuthors ();

		Glib::ustring::size_type comma = authors.find (",");
		Glib::ustring::size_type space = authors.find (" ");
		Glib::ustring::size_type snip = Glib::ustring::npos;

		if (comma != Glib::ustring::npos)
			snip = comma;
		if (space != Glib::ustring::npos && space < comma)
			snip = space;

		if (snip != Glib::ustring::npos)
			authors = authors.substr(0, snip);


		if (authors.size() > maxlen - 2) {
			authors = authors.substr(0, maxlen - 2);
		}

		// Should:
		// Truncate it at the first "et al", "and", or ","

		name = authors + year;
	} else if (!filename_.empty ()) {
		Glib::ustring filename = Gio::File::create_for_uri(filename_)->query_info()->get_display_name();

		Glib::ustring::size_type periodpos = filename.find_last_of (".");
		if (periodpos != std::string::npos) {
			filename = filename.substr (0, periodpos);
		}

		name = filename;
		if (name.size() > maxlen) {
			name = name.substr(0, maxlen);
		}

	} else {
		name = defaultKey_;
	}

	// Don't confuse LaTeX
	name = Utility::strip (name, " ");
	name = Utility::strip (name, "&");
	name = Utility::strip (name, "$");
	name = Utility::strip (name, "%");
	name = Utility::strip (name, "#");
	name = Utility::strip (name, "_");
	name = Utility::strip (name, "{");
	name = Utility::strip (name, "}");
	name = Utility::strip (name, ",");
	name = Utility::strip (name, "@");

	return name;
}


void Document::setThumbnail (Glib::RefPtr<Gdk::Pixbuf> thumb)
{
	thumbnail_ = thumb;
	if (view_)
		view_->updateDoc(this);
}


void Document::setupThumbnail ()
{
	if (!loadingthumb_) {
		loadingthumb_ = Utility::getThemeIcon ("image-loading");
		if (!loadingthumb_)
			loadingthumb_ = Gdk::Pixbuf::create_from_file
				(Utility::findDataFile ("unknown-document.png"));

		if (loadingthumb_) {
			/* FIXME magic number duplicated elsewhere */
			float const desiredwidth = 64.0 + 9;
			int oldwidth = loadingthumb_->get_width ();
			int oldheight = loadingthumb_->get_height ();
			int newwidth = (int)desiredwidth;
			int newheight = (int)((float)oldheight * (desiredwidth / (float)oldwidth));
			loadingthumb_ = loadingthumb_->scale_simple (
				newwidth, newheight, Gdk::INTERP_BILINEAR);
		}

	}
	thumbnail_ = loadingthumb_;


	ThumbnailGenerator::instance().registerRequest (filename_, this);
}


Glib::ustring const & Document::getKey() const
{
	return key_;
}


Glib::ustring const & Document::getFileName() const
{
	return filename_;
}


Glib::ustring const & Document::getRelFileName() const
{
	return relfilename_;
}


void Document::setFileName (Glib::ustring const &filename)
{
	ThumbnailGenerator::instance().deregisterRequest (this);

	if (filename != filename_) {
		filename_ = filename;
		setupThumbnail ();
	} else if (!thumbnail_) {
		setupThumbnail ();
	}
}

Glib::ustring const & Document::getNotes () const
{
	return notes_;
}

void Document::setNotes (Glib::ustring const &notes)
{
	if (notes != notes_)
		notes_ = notes;
}


void Document::updateRelFileName (Glib::ustring const &libfilename)
{
	const Glib::RefPtr<Gio::File> doc_file = Gio::File::create_for_uri(filename_);
	Glib::RefPtr<Gio::File> lib_path = Gio::File::create_for_uri(libfilename)->get_parent();

	bool doc_is_relative_to_library = true;
	std::string relative_path;
	std::string up_dir_level;

	if ( !libfilename.empty() && lib_path->get_uri_scheme() == doc_file->get_uri_scheme() ) {

		for( ;; ) {
			relative_path = lib_path->get_relative_path(doc_file);
			if (!relative_path.empty()) {
				relative_path = up_dir_level + relative_path;
				break;
			}

			lib_path = lib_path->get_parent();
			up_dir_level += "../";
			if (!lib_path) {
				doc_is_relative_to_library = false;
				break;
			}
		}

	} else {
		doc_is_relative_to_library = false;
	}

	if ( doc_is_relative_to_library ) {
		relfilename_ = relative_path;
		DEBUG ("Set relfilename_ '%1'", relfilename_);
	} else {
		relfilename_ = "";
		DEBUG ("Not relative");
	}
}

void Document::setKey (Glib::ustring const &key)
{
	key_ = key;
}


std::vector<int>& Document::getTags()
{
	return tagUids_;
}

void Document::setTag(int uid)
{
	if (hasTag(uid)) {
		std::ostringstream num;
		num << uid;
	} else {
		tagUids_.push_back(uid);
	}
}


void Document::clearTag(int uid)
{
	std::vector<int>::iterator location =
		std::find(tagUids_.begin(), tagUids_.end(), uid);

	if (location != tagUids_.end())
		tagUids_.erase(location);
}


void Document::clearTags()
{
	tagUids_.clear();
}


bool Document::hasTag(int uid)
{
	return std::find(tagUids_.begin(), tagUids_.end(), uid) != tagUids_.end();
}


using Utility::writeBibKey;

/**
 * Temporarily duplicating functionality in printBibtex and 
 * writeBibtex -- the difference is that writeBibtex requires a 
 * Library reference in order to resolve tag uids to names.
 * In order to be usable from PythonDocument printBibtex just 
 * doesn't bother printing tags at all.
 *
 * This will get fixed when the ill-conceived tag ID system is 
 * replaced with lists of strings
 */
Glib::ustring Document::printBibtex (
	bool const useBraces,
	bool const utf8)
{
	std::ostringstream out;

	// BibTeX values cannot be larger than 1000 characters - should make sure of this
	// We should strip illegal characters from key in a predictable way
	out << "@" << bib_.getType() << "{" << key_ << ",\n";

	BibData::ExtrasMap extras = bib_.getExtras ();
	BibData::ExtrasMap::iterator it = extras.begin ();
	BibData::ExtrasMap::iterator const end = extras.end ();
	for (; it != end; ++it) {
		// Exceptions to useBraces are editor and author because we
		// don't want "Foo, B.B. and John Bar" to be literal
		writeBibKey (
			out,
			(*it).first,
			(*it).second,
			((*it).first.lowercase () != "editor") && useBraces, utf8);
	}

	// Ideally should know what's a list of human names and what's an
	// institution name be doing something different for non-human-name authors?
	writeBibKey (out, "author",  bib_.getAuthors(), false, utf8);
	writeBibKey (out, "title",   bib_.getTitle(), useBraces, utf8);
	writeBibKey (out, "journal", bib_.getJournal(), useBraces, utf8);
	writeBibKey (out, "volume",  bib_.getVolume(), false, utf8);
	writeBibKey (out, "number",  bib_.getIssue(), false, utf8);
	writeBibKey (out, "pages",   bib_.getPages(), false, utf8);
	writeBibKey (out, "year",    bib_.getYear(), false, utf8);
	writeBibKey (out, "doi",    bib_.getDoi(), false, utf8);

	out << "}\n\n";

	return out.str();
}


void Document::writeBibtex (
	Library const &lib,
	std::ostringstream& out,
	bool const usebraces,
	bool const utf8)
{
	// BibTeX values cannot be larger than 1000 characters - should make sure of this
	// We should strip illegal characters from key in a predictable way
	out << "@" << bib_.getType() << "{" << key_ << ",\n";

	BibData::ExtrasMap extras = bib_.getExtras ();
	BibData::ExtrasMap::iterator it = extras.begin ();
	BibData::ExtrasMap::iterator const end = extras.end ();
	for (; it != end; ++it) {
		// Exceptions to usebraces are editor and author because we
		// don't want "Foo, B.B. and John Bar" to be literal
		writeBibKey (
			out,
			(*it).first,
			(*it).second,
			((*it).first.lowercase () != "editor") && usebraces, utf8);
	}

	// Ideally should know what's a list of human names and what's an
	// institution name be doing something different for non-human-name authors?
	writeBibKey (out, "author",  bib_.getAuthors(), false, utf8);
	writeBibKey (out, "title",   bib_.getTitle(), usebraces, utf8);
	writeBibKey (out, "journal", bib_.getJournal(), usebraces, utf8);
	writeBibKey (out, "volume",  bib_.getVolume(), false, utf8);
	writeBibKey (out, "number",  bib_.getIssue(), false, utf8);
	writeBibKey (out, "pages",   bib_.getPages(), false, utf8);
	writeBibKey (out, "year",    bib_.getYear(), false, utf8);
	writeBibKey (out, "doi",    bib_.getDoi(), false, utf8);
	
	if (tagUids_.size () > 0) {
		out << "\ttags = \"";
		std::vector<int>::iterator tagit = tagUids_.begin ();
		std::vector<int>::iterator const tagend = tagUids_.end ();
		for (; tagit != tagend; ++tagit) {
			if (tagit != tagUids_.begin ())
				out << ", ";
			out << lib.getTagList()->getName(*tagit);
		}
		out << "\"\n";
	}

	out << "}\n\n";
}

void Document::writeXML(xmlTextWriterPtr writer) {
    xmlTextWriterStartElement(writer, BAD_CAST LIB_ELEMENT_DOC);

    /* Prefer to use write only relative filenames */
    if (!relfilename_.empty()) {
    	xmlTextWriterWriteElement(writer, BAD_CAST LIB_ELEMENT_DOC_REL_FILENAME, BAD_CAST relfilename_.c_str());
    }
    else {
    	xmlTextWriterWriteElement(writer, BAD_CAST LIB_ELEMENT_DOC_FILENAME, BAD_CAST filename_.c_str());
    }
    xmlTextWriterWriteElement(writer, BAD_CAST LIB_ELEMENT_DOC_KEY, BAD_CAST getKey().c_str());
    xmlTextWriterWriteElement(writer, BAD_CAST LIB_ELEMENT_DOC_NOTES, BAD_CAST getNotes().c_str());

	std::vector<int> docvec = getTags();
    for (std::vector<int>::iterator it = docvec.begin(); it != docvec.end(); ++it) {
        xmlTextWriterWriteFormatElement(writer, BAD_CAST LIB_ELEMENT_DOC_TAG, "%d", (*it));
	}

    getBibData().writeXML(writer);

    xmlTextWriterEndElement(writer);
}

void Document::readXML(xmlNodePtr docNode) {
    for (xmlNodePtr child = xmlFirstElementChild(docNode); child; child
            = xmlNextElementSibling(child)) {
        if (nodeNameEq(child, LIB_ELEMENT_DOC_TAG)) {
            SET_FROM_NODE_MAP(setTag, child, atoi);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_REL_FILENAME)) {
            SET_FROM_NODE(setRelFileName, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_FILENAME)) {
            SET_FROM_NODE(setFileName, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_KEY)) {
            SET_FROM_NODE(setKey, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_NOTES)) {
            SET_FROM_NODE(setNotes, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_AUTHORS)) {
            SET_FROM_NODE(getBibData().setAuthors, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_DOI)) {
            SET_FROM_NODE(getBibData().setDoi, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_EXTRA)) {
            char* extraKey = STR xmlGetProp(child, XSTR LIB_ELEMENT_DOC_BIB_EXTRA_KEY);
            char* extraText = STR xmlNodeGetContent(child);
            getBibData().addExtra(extraKey, extraText);
            xmlFree(extraKey);
            xmlFree(extraText);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_JOURNAL)) {
            SET_FROM_NODE(getBibData().setJournal, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_NUMBER)) {
            SET_FROM_NODE(getBibData().setIssue, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_PAGES)) {
            SET_FROM_NODE(getBibData().setPages, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_TITLE)) {
            SET_FROM_NODE(getBibData().setTitle, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_TYPE)) {
            SET_FROM_NODE(getBibData().setType, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_VOLUME)) {
            SET_FROM_NODE(getBibData().setVolume, child);
        } else if (nodeNameEq(child, LIB_ELEMENT_DOC_BIB_YEAR)) {
            SET_FROM_NODE(getBibData().setYear, child);
        }
    }
}

bool Document::readPDF ()
{
	if (filename_.empty()) {
		DEBUG ("Document::readPDF: has no filename");
		return false;
	}

	std::string contentType = Gio::File::create_for_uri(filename_)->query_info("standard::content-type")->get_content_type();
	if (contentType != "application/pdf")
		return false;

	GError *error = NULL;
	PopplerDocument *popplerdoc = poppler_document_new_from_file (filename_.c_str(), NULL, &error);
	if (popplerdoc == NULL) {
		DEBUG ("Document::readPDF: Failed to load '%1'", filename_);
		g_error_free (error);
		return false;
	}

	Glib::ustring textdump;
	int num_pages = poppler_document_get_n_pages (popplerdoc);

	if (num_pages == 0) {
		DEBUG ("Document::readPDF: No pages in '%1'", filename_);
		return false;
	}

	// Read the first page
	PopplerPage *page;
	page = poppler_document_get_page (popplerdoc, 0);
	textdump += poppler_page_get_text(page);
	g_object_unref (page);

	// When we read the first page, see if it has the doc info
	bib_.guessYear (textdump);
	bib_.guessDoi (textdump);
	bib_.guessArxiv (textdump);

	//Try to extract PDF metadata
	char *pdfauthor_c = poppler_document_get_author(popplerdoc);
	if (pdfauthor_c) {
		Glib::ustring pdfauthor = pdfauthor_c;
		if (pdfauthor != "" && pdfauthor.find(" ") != Glib::ustring::npos) {
			//If author contains more than one word, it might be sensible
			//Some bad examples: "Author", "jol", "IEEE",
			//"U-STAR\bgogul,S-1-5-21-2879401181-1713613690-3240760954-1005"
			
			bib_.setAuthors(pdfauthor);
		}
		DEBUG ("pdfauthor: %1", pdfauthor);
	}

	char *pdftitle_c = poppler_document_get_title(popplerdoc);
	if (pdftitle_c) {
		Glib::ustring pdftitle = pdftitle_c;
		if (pdftitle != "" && pdftitle.find(" ") != Glib::ustring::npos) {
			//If title contains more than one word, it might be sensible
			//Some bad examples: "Title", "ssl-attacks.dvi",
			//"doi:10.1016/j.scico.2005.02.009", "MAIN", "24690003",
			//"untitled", "PII: 0304-3975(96)00072-2"
			
			bib_.setTitle(pdftitle);
		}
		DEBUG ("pdftitle: %1", pdftitle);
	}
	

	g_object_unref (popplerdoc);

	//DEBUG ("%1", textdump);
	return !(textdump.empty ());
}


bool Document::canGetMetadata ()
{
	return _global_plugins->canResolve(*this);
}


/* [bert] hack this to handle searches with a space like iTunes. For
 * now we just treat the whole search string as the boolean "AND" of
 * each individual component, and check the whole string against each
 * document.
 *
 * A better way to do this would be to change the whole search
 * procedure to narrow the search progressively by term, so that
 * additional terms search only through an already-filtered list.
 * But this would require changing higher-level code, for example, 
 * the DocumentView::isVisible() method that calls this function would
 * have to change substantially.
 */

bool Document::matchesSearch (Glib::ustring const &search)
{
    /* This is a bit of a hack, I guess, but it's a low-impact way of 
     * implementing the change. If the search term contains a space, I 
     * iteratively decompose it into substrings and pass those onto
     * this function. If anything doesn't match, we return a failure.
     */
    if (search.find(' ') != Glib::ustring::npos) {
        Glib::ustring::size_type p1 = 0;
        Glib::ustring::size_type p2;

        do {

            /* Find the next space in the string, if any.
             */
            p2 = search.find(' ', p1);

            /* Extract the appropriate substring.
             */
            Glib::ustring const searchTerm = search.substr(p1, p2);

            /* If the term is empty, ignore it and move on. It might just be a 
             * trailing or duplicate space character.
             */
            if (searchTerm.empty()) {
                break;
            }

            /* Now that we have the substring, which is guaranteed to be
             * free of spaces, we can pass it recursively into this function.
             * If the term does NOT match, fail the entire comparison right away.
             */
            if (!matchesSearch(searchTerm)) {
                return false;
            }

            p1 = p2 + 1;		/* +1 to skip over the space */

        } while (p2 != Glib::ustring::npos); /* Terminate at end of string */
        return true;		/* All matched, so OK. */
    }

    Glib::ustring const searchNormalised = search.casefold();

    FieldMap fields = getFields ();
    FieldMap::iterator fieldIter = fields.begin ();
    FieldMap::iterator const fieldEnd = fields.end ();
    for (; fieldIter != fieldEnd; ++fieldIter) {
        if (fieldIter->second.casefold().find(searchNormalised) != Glib::ustring::npos)
            return true;
    }

    if (notes_.casefold().find(searchNormalised) != Glib::ustring::npos)
        return true;

    if (key_.casefold().find(searchNormalised) != Glib::ustring::npos)
        return true;

    return false;
}

/*
 * Returns true on success
 */
bool Document::getMetaData ()
{
	if (_global_prefs->getWorkOffline())
		return false;

	bool success = false;

	success = _global_plugins->resolveMetadata(*this);

	/*
	 * Set up the key if it was never set to begin with
	 */
	if (success) {
		if (getKey().substr(0, defaultKey_.size()) == defaultKey_)
			setKey(generateKey ());
	}

	return success;
}


void Document::renameFromKey ()
{
	if (getFileName().empty () || getKey().empty ())
		return;

	Glib::RefPtr<Gio::File> oldfile = Gio::File::create_for_uri(getFileName());

	Glib::ustring shortname = oldfile->query_info()->get_display_name();
	DEBUG ("Shortname = %1", shortname);
	Glib::RefPtr<Gio::File> parentdir = oldfile->get_parent();

	Glib::ustring::size_type pos = shortname.rfind (".");
	Glib::ustring extension = "";
	if (pos != Glib::ustring::npos)
		extension = shortname.substr (pos, shortname.length() - 1);

	Glib::ustring newfilename = getKey() + extension;
	DEBUG ("Newfilename = %1", newfilename);

	Glib::RefPtr<Gio::File> newfile = parentdir->get_child(newfilename);

	try {
		oldfile->move(newfile);
		setFileName (newfile->get_uri ());
	} catch (Gio::Error &ex) {
		Utility::exceptionDialog (&ex,
			String::ucompose (_("Moving '%1' to '%2'"),
				oldfile->get_uri (),
				newfile->get_uri ())
			);
	}
}


void Document::setField (Glib::ustring const &field, Glib::ustring const &value)
{
	DEBUG ("%1 : %2", field, value);
	if (field == "doi")
		bib_.setDoi (value);
	else if (field.lowercase() == "title")
		bib_.setTitle (value);
	else if (field.lowercase() == "volume")
		bib_.setVolume (value);
	else if (field.lowercase() == "number")
		bib_.setIssue (value);
	else if (field.lowercase() == "journal")
		bib_.setJournal (value);
	else if (field.lowercase() == "author")
		bib_.setAuthors (value);
	else if (field.lowercase() == "year")
		bib_.setYear (value);
	else if (field.lowercase() == "pages")
		bib_.setPages (value);
	else if (field == "key")
		setKey (value);
	else {
		/* The extras map uses a case-folding comparator */
		bib_.extras_[field] = value;
	}
}


/* Can't be const because of std::map[] */
Glib::ustring Document::getField (Glib::ustring const &field)
{
	if (field == "doi")
		return bib_.getDoi ();
	else if (field == "title")
		return bib_.getTitle ();
	else if (field == "volume")
		return bib_.getVolume ();
	else if (field == "number")
		return bib_.getIssue ();
	else if (field == "journal")
		return bib_.getJournal ();
	else if (field == "author")
		return bib_.getAuthors ();
	else if (field == "year")
		return bib_.getYear ();
	else if (field == "pages")
		return bib_.getPages ();
	else if (field == "key")
		return getKey();
	else {
		if (bib_.extras_.find(field) != bib_.extras_.end()) {
			const Glib::ustring _field = field;
			return bib_.extras_[_field];
		} else {
			DEBUG ("Document::getField: WARNING: unknown field %1", field);
			throw std::range_error("Document::getField: unknown field");
		}
	}
}


bool Document::hasField (Glib::ustring const &field) const
{
	if (field == "doi")
		return !bib_.getDoi ().empty();
	else if (field == "title")
		return !bib_.getTitle ().empty();
	else if (field == "volume")
		return !bib_.getVolume ().empty();
	else if (field == "number")
		return !bib_.getIssue ().empty();
	else if (field == "journal")
		return !bib_.getJournal ().empty();
	else if (field == "author")
		return !bib_.getAuthors ().empty();
	else if (field == "year")
		return !bib_.getYear ().empty();
	else if (field == "pages")
		return !bib_.getPages ().empty();
	else {
		if (bib_.extras_.find(field) != bib_.extras_.end())
			return true;
		else
			return false;
	}
}


/*
 * Metadata fields.  Does not include document key or type
 */
std::map <Glib::ustring, Glib::ustring> Document::getFields ()
{
	std::map <Glib::ustring, Glib::ustring> fields;

	if (!bib_.getDoi ().empty())
		fields["doi"] = bib_.getDoi();

	if (!bib_.getTitle ().empty())
		fields["title"] = bib_.getTitle();

	if (!bib_.getVolume ().empty())
		fields["volume"] = bib_.getVolume();

	if (!bib_.getIssue ().empty())
		fields["number"] = bib_.getIssue();

	if (!bib_.getJournal ().empty())
		fields["journal"] = bib_.getJournal();

	if (!bib_.getAuthors ().empty())
		fields["author"] = bib_.getAuthors();

	if (!bib_.getYear ().empty())
		fields["year"] = bib_.getYear();

	if (!bib_.getPages ().empty())
		fields["pages"] = bib_.getPages();

		
	BibData::ExtrasMap::iterator it = bib_.extras_.begin ();
	BibData::ExtrasMap::iterator end = bib_.extras_.end ();
	for (; it != end; ++it) {
		fields[(*it).first] = (*it).second;
	}

	return fields;
}


void Document::clearFields ()
{
	bib_.extras_.clear ();
	setField ("doi", "");
	setField ("title", "");
	setField ("volume", "");
	setField ("number", "");
	setField ("journal", "");
	setField ("author", "");
	setField ("year", "");
	setField ("pages", "");
}


bool Document::parseBibtex (Glib::ustring const &bibtex)
{
	BibUtils::param p;
	BibUtils::bibl b;
	BibUtils::bibl_init( &b );
	BibUtils::bibl_initparams( &p, BibUtils::FORMAT_BIBTEX, BIBL_MODSOUT, BibUtils::progname);

	try {
		/* XXX should convert this to latin1? */
		BibUtils::biblFromString (b, bibtex, p);
		if (b.n != 1)
			return false;

		Document newdoc = BibUtils::parseBibUtils (b.ref[0]);

		getBibData().mergeIn (newdoc.getBibData());	
		
		BibUtils::bibl_free( &b );
		return true;
	} catch (Glib::Error& ex) {
		BibUtils::bibl_free( &b );
		Utility::exceptionDialog (&ex, _("Parsing BibTeX"));
	}
	return false;
}
