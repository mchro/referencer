
#ifndef CASEFOLDCOMPARE_H
#define CASEFOLDCOMPARE_H

struct casefoldCompare {
	bool operator () ( const Glib::ustring &lhs, const Glib::ustring &rhs ) const {
		return lhs.casefold() < rhs.casefold();
	}
};

#endif
