// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape Widget Utilities
 *
 * Authors:
 *   Bryce W. Harrington <brycehar@bryceharrington.org>
 *   bulia byak <buliabyak@users.sf.net>
 *
 * Copyright (C) 2003 Bryce W. Harrington
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstring>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/grid.h>

#include "selection.h"

#include "spw-utilities.h"

/**
 * Creates a label widget with the given text, at the given col, row
 * position in the table.
 */
Gtk::Label * spw_label(Gtk::Grid *table, const gchar *label_text, int col, int row, Gtk::Widget* target)
{
  Gtk::Label *label_widget = new Gtk::Label();
  g_assert(label_widget != nullptr);
  if (target != nullptr) {
    label_widget->set_text_with_mnemonic(label_text);
    label_widget->set_mnemonic_widget(*target);
  } else {
    label_widget->set_text(label_text);
  }
  label_widget->show();

  label_widget->set_halign(Gtk::ALIGN_START);
  label_widget->set_valign(Gtk::ALIGN_CENTER);
  label_widget->set_margin_start(4);
  label_widget->set_margin_end(4);

  table->attach(*label_widget, col, row, 1, 1);

  return label_widget;
}

/**
 * Creates a horizontal layout manager with 4-pixel spacing between children
 * and space for 'width' columns.
 */
Gtk::Box * spw_hbox(Gtk::Grid * table, int width, int col, int row)
{
  /* Create a new hbox with a 4-pixel spacing between children */
  Gtk::Box *hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
  g_assert(hb != nullptr);
  hb->show();
  hb->set_hexpand();
  hb->set_halign(Gtk::ALIGN_FILL);
  hb->set_valign(Gtk::ALIGN_CENTER);
  table->attach(*hb, col, row, width, 1);

  return hb;
}

/**
 * Finds the descendant of w which has the data with the given key and returns the data, or NULL if there's none.
 */
gpointer sp_search_by_data_recursive(GtkWidget *w, gpointer key)
{
    gpointer r = nullptr;

    if (w && G_IS_OBJECT(w)) {
        r = g_object_get_data(G_OBJECT(w), (gchar *) key);
    }
    if (r) return r;

    if (GTK_IS_CONTAINER(w)) {
            std::vector<Gtk::Widget*> children = Glib::wrap(GTK_CONTAINER(w))->get_children();
        for (auto i:children) {
            r = sp_search_by_data_recursive(GTK_WIDGET(i->gobj()), key);
            if (r) return r;
        }
    }

    return nullptr;
}

/**
 * Returns a named descendent of parent, which has the given name, or nullptr if there's none.
 *
 * \param[in] parent The widget to search
 * \param[in] name   The name of the desired child widget
 *
 * \return The specified child widget, or nullptr if it cannot be found
 */
Gtk::Widget *
sp_search_by_name_recursive(Gtk::Widget *parent, const Glib::ustring& name)
{
    auto parent_bin = dynamic_cast<Gtk::Bin *>(parent);
    auto parent_container = dynamic_cast<Gtk::Container *>(parent);

    if (parent && parent->get_name() == name) {
        return parent;
    }
    else if (parent_bin) {
        auto child = parent_bin->get_child();
        return sp_search_by_name_recursive(child, name);
    }
    else if (parent_container) {
        auto children = parent_container->get_children();

        for (auto child : children) {
            auto tmp = sp_search_by_name_recursive(child, name);

            if (tmp) {
                return tmp;
            }
        }
    }

    return nullptr;
}

/**
 * Returns the descendant of w which has the given key and value pair, or NULL if there's none.
 */
GtkWidget *sp_search_by_value_recursive(GtkWidget *w, gchar *key, gchar *value)
{
    gchar *r = nullptr;

    if (w && G_IS_OBJECT(w)) {
        r = (gchar *) g_object_get_data(G_OBJECT(w), key);
    }
    if (r && !strcmp (r, value)) return w;

    if (GTK_IS_CONTAINER(w)) {
                std::vector<Gtk::Widget*> children = Glib::wrap(GTK_CONTAINER(w))->get_children();
        for (auto i:children) {
            GtkWidget *child = sp_search_by_value_recursive(GTK_WIDGET(i->gobj()), key, value);
            if (child) return child;
        }
    }

    return nullptr;
}

/**
 * This function traverses a tree of widgets descending into bins and containers.
 * It stops and returns a pointer to the first child widget for which 'eval' evaluates to true.
 * If 'eval' never returns true then this function visits all widgets and returns nullptr.
 *
 * \param[in] widget The widget to start traversal from - top of the tree
 * \param[in] eval   The callback invoked for each visited widget
 *
 * \return The widget for which 'eval' returned true, or nullptr otherwise.
 * Note: it could be a starting widget too.
 */
Gtk::Widget* sp_traverse_widget_tree(Gtk::Widget* widget, const std::function<bool (Gtk::Widget*)>& eval) {
    if (!widget) return nullptr;

    if (eval(widget)) return widget;

    if (auto bin = dynamic_cast<Gtk::Bin*>(widget)) {
        return sp_traverse_widget_tree(bin->get_child(), eval);
    }
    else if (auto container = dynamic_cast<Gtk::Container*>(widget)) {
        auto&& children = container->get_children();
        for (auto child : children) {
            if (auto found = sp_traverse_widget_tree(child, eval)) {
                return found;
            }
        }
    }

    return nullptr;
}

/**
 * This function traverses a tree of widgets searching for first focusable widget.
 *
 * \param[in] widget The widget to start traversal from - top of the tree
 *
 * \return The first focusable widget or nullptr if none are focusable.
 */
Gtk::Widget* sp_find_focusable_widget(Gtk::Widget* widget) {
    return sp_traverse_widget_tree(widget, [](Gtk::Widget* w) { return w->get_can_focus(); });
}


Glib::ustring sp_get_action_target(Gtk::Widget* widget) {
    Glib::ustring target;

    if (widget && GTK_IS_ACTIONABLE(widget->gobj())) {
        auto variant = gtk_actionable_get_action_target_value(GTK_ACTIONABLE(widget->gobj()));
        auto type = variant ? g_variant_get_type_string(variant) : nullptr;
        if (type && strcmp(type, "s") == 0) {
            target = g_variant_get_string(variant, nullptr);
        }
    }

    return target;
}
/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
