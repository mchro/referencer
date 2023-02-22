
#include <iostream>

#include <gtkmm.h>

#include "Utility.h"

#include "DocumentView.h"
#include "DocumentCellRenderer.h"

void DocumentCellRenderer::get_preferred_width_vfunc(Gtk::Widget& widget,
                                       int& min_w,
                                       int& nat_w) const
{
    Document *doc = (Document *) property_document_.get_value ();
    Glib::RefPtr<Gdk::Pixbuf> thumb = doc->getThumbnail ();
    gint pixbuf_width  = thumb->get_width ();
    gint calc_width = pixbuf_width;

    min_w = calc_width;
    nat_w = calc_width;
}

void DocumentCellRenderer::get_preferred_height_vfunc(Gtk::Widget& widget,
                                        int& min_h,
                                        int& nat_h) const
{
    Document *doc = (Document *) property_document_.get_value ();
    Glib::RefPtr<Gdk::Pixbuf> thumb = doc->getThumbnail ();
    gint pixbuf_height = thumb->get_height ();
    gint calc_height = pixbuf_height;
    gint height;

    if (calc_height < maxHeight)
        height = calc_height;
    else
        height = maxHeight;

    min_h = height;
    nat_h = height;

}

void DocumentCellRenderer::render_vfunc( const Cairo::RefPtr<Cairo::Context>& cr,
                           Gtk::Widget& widget,
                           const Gdk::Rectangle& background_area,
                           const Gdk::Rectangle& cell_area,
                           Gtk::CellRendererState flags )
{
    Document *doc = (Document *) property_document_.get_value ();
    Glib::RefPtr<Gdk::Pixbuf> thumb = doc->getThumbnail ();

    cr->save();
    cr->translate(cell_area.get_x(), cell_area.get_y());
    Gdk::Cairo::set_source_pixbuf(cr, thumb, 0, 0);
    cr->rectangle(0, 0, thumb->get_width(), thumb->get_height());
    cr->paint();
    cr->restore();

    if (doc == docview_->hoverdoc_) {
        flags |= Gtk::CELL_RENDERER_PRELIT;
    }
}
