# coding=utf-8
"""
Make sure the webbrowser extension is working.
"""
import launch_webbrowser
from inkex.tester import TestCase

class TestWebsiteOpen(TestCase):
    """Test Website openner with dummy web browser"""
    effect_class = launch_webbrowser.ThreadWebsite

    def setUp(self):
        super(TestWebsiteOpen, self).setUp()
        launch_webbrowser.BROWSER = 'echo %s'

    def tearDown(self):
        super(TestWebsiteOpen, self).tearDown()
        launch_webbrowser.BROWSER = None

    def test_open(self):
        """Test website opens"""
        self.effect_class(['--url=https://inkscape.org/']).run()
        # There's no way to test the output yet (stdout).
