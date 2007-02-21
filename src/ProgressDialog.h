
#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <gtkmm.h>

class ProgressDialog {
	public:
	ProgressDialog ();
	~ProgressDialog ();

	void setLabel (Glib::ustring &text);
	void start ();	
	void update (double status);
	void update ();
	void finish ();
	
	private:
	Gtk::ProgressDialogBar *progress_;
	Gtk::Label *label_;
	Gtk::Dialog *dialog_;
	bool finished_;
	Glib::Thread *loopthread_;
}

#endif

/*

{

	ProgressDialog prog (false);

	ProgressDialog.setLabel ("Working, please show some fucking patience for once");
	ProgressDialog.start ();

	for (int i = 0; i < 100; ++i) {
		ProgressDialog.update ((double)i / 100.0);
		fuck_about_a_bit (i);
	}

	ProgressDialog.finish ();

}

*/
