// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * anchor-selector.h
 *
 *  Created on: Mar 22, 2012
 *      Author: denis
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef ANCHOR_SELECTOR_H_
#define ANCHOR_SELECTOR_H_

#include <gtkmm/bin.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/grid.h>

namespace Inkscape {
namespace UI {
namespace Widget {

class AnchorSelector : public Gtk::Bin
{
private:
	Gtk::ToggleButton  _buttons[9];
	int                _selection;
	Gtk::Grid          _container;

	sigc::signal<void> _selectionChanged;

	void setupButton(const Glib::ustring &icon, Gtk::ToggleButton &button);
	void btn_activated(int index);

public:

	int getHorizontalAlignment() { return _selection % 3; }
	int getVerticalAlignment() { return _selection / 3; }

	sigc::signal<void> &on_selectionChanged() { return _selectionChanged; }

	void setAlignment(int horizontal, int vertical);

	AnchorSelector();
	~AnchorSelector() override;
};

} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif /* ANCHOR_SELECTOR_H_ */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
