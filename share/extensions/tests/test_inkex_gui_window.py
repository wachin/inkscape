# coding=utf-8
#
# Copyright 2022 Martin Owens <doctormo@geek-2.com>
#
# This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>
#
"""
Test the various features of a Gtk/Inkex Window.
"""

import os
import sys
import time
import pytest

from inkex.tester import TestCase
from inkex.utils import DependencyError

try:
    from inkex.gui.tester import MainLoopProtection
    from inkex.gui import GtkApp, Window, ChildWindow, asyncme
except DependencyError:
    Window = object
    ChildWindow = object
    GtkApp = None


class BadNamedWindow(Window):
    name = "bad-name"


class BasicWindow(Window):
    name = "basic-window"


class ChildWindow(ChildWindow):
    name = "child-window"


@pytest.mark.skipif(GtkApp is None, reason="PyGObject is required")
class GtkWindowTest(TestCase):
    """Tests for GUI Window"""

    def construct_app(self, windows=()):
        """Create a gtk app based on some inputs"""
        return type(
            "_GtkApp",
            (GtkApp,),
            {
                "app_name": "window-test",
                "windows": list(windows),
                "ui_dir": self.datadir(),
                "prefix": "ui",
                "ui_file": "window-test",
            },
        )

    def test_window(self):
        """Various consruction errors"""
        app = self.construct_app([BadNamedWindow])

    def test_load_window_extract(self):
        """Test extracting widgets"""
        app = self.construct_app([BasicWindow])()
        widget = app.load_window_extract("basic-window")
        self.assertEqual(Window.get_widget_name(widget), "box1")
        # original window widget is untouched
        self.assertTrue(list(app.window.window.get_children()))
        # Destroy origial widgets
        app.window.exit(widget)

    def test_child_window(self):
        """Test the loading of child windows"""
        app = self.construct_app([BasicWindow, ChildWindow])()
        window = app.window
        child = window.load_window("child-window")
        self.assertEqual(Window.get_widget_name(child.widget("box2")), "box2")
        self.assertEqual(child.name, "child-window")
        child.exit(child)

    def test_if_widget(self):
        """Test getting a widget with a fake fallback"""
        app = self.construct_app([BasicWindow])()
        faker = app.window.if_widget("fake")
        self.assertFalse(faker)
        self.assertFalse(faker.get_name())

    def test_replace_widget(self):
        """Replace a widget in a window"""
        app = self.construct_app([BasicWindow])()
        app.window.replace("button1", "button2")
