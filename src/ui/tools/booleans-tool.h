// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * A tool for building shapes.
 */
/* Authors:
 *   Martin Owens
 *
 * Copyright (C) 2022 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOLS_BOOLEANS_TOOL
#define INKSCAPE_UI_TOOLS_BOOLEANS_TOOL

#include "ui/tools/tool-base.h"

class SPDesktop;

namespace Inkscape {
class BooleanBuilder;

namespace UI {
namespace Tools {

class InteractiveBooleansTool : public ToolBase {
public:

    InteractiveBooleansTool(SPDesktop *desktop);
    ~InteractiveBooleansTool() override;

    void switching_away(const std::string &new_tool) override;

    // Preferences set
    void set(const Inkscape::Preferences::Entry& val) override;

    // Undo/redo catching
    bool catch_undo(bool redo) override;

    // Catch empty selections
    bool is_ready() const override;

    // Event functions
    bool root_handler(GdkEvent* event) override;

    void shape_commit();
    void shape_cancel();
private:
    void update_status();
    void change_mode(bool setup);
    bool should_add(int state) const;

    bool event_button_press_handler(GdkEvent* event);
    bool event_button_release_handler(GdkEvent* event);
    bool event_motion_handler(GdkEvent* event, bool add);
    bool event_key_press_handler(GdkEvent* event);

    std::unique_ptr<BooleanBuilder> boolean_builder;

    sigc::connection _sel_modified;
    sigc::connection _sel_changed;

    bool to_commit = false;
};

} // namespace Tools
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_TOOLS_BOOLEANS_TOOL
