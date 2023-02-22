
#include <gtkmm.h>

#include "Document.h"

class DocumentView;

class DocumentCellRenderer : public Gtk::CellRendererPixbuf {

private:
    Glib::Property< void* > property_document_; 
    static int const maxHeight = 128;
    DocumentView *docview_;

public:
    DocumentCellRenderer(DocumentView *docview)
    :
    Glib::ObjectBase(typeid(DocumentCellRenderer)),
    Gtk::CellRendererPixbuf(),
    property_document_ (*this, "document"),
    docview_(docview)
    {
        property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
        property_xpad() = 0;
        property_ypad() = 0;
    }   

    Glib::PropertyProxy< void* > property_document() {return property_document_.get_proxy();}

protected:

    void get_preferred_width_vfunc(Gtk::Widget& widget,
                                           int& min_w,
                                           int& nat_w) const override;

    void get_preferred_height_vfunc(Gtk::Widget& widget,
                                            int& min_h,
                                            int& nat_h) const override;

    void render_vfunc( const Cairo::RefPtr<Cairo::Context>& cr,
                               Gtk::Widget& widget,
                               const Gdk::Rectangle& background_area,
                               const Gdk::Rectangle& cell_area,
                               Gtk::CellRendererState flags ) override;
};
