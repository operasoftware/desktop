# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host import Host
from blinkpy.web_tests.controllers import repaint_overlay

LAYER_TREE = """{
  "layers": [
    {
      "name": "layer1",
      "bounds": [800.00, 600.00],
    }, {
      "name": "layer2",
      "position": [8.00, 80.00],
      "bounds": [800.00, 600.00],
      "contentsOpaque": true,
      "drawsContent": true,
      "invalidations": [
        [8, 108, 100, 100],
        [0, 216, 800, 100],
      ]
    }
  ]
}
"""


class TestRepaintOverlay(unittest.TestCase):
    def test_result_contains_repaint_rects(self):
        self.assertTrue(
            repaint_overlay.result_contains_repaint_rects(LAYER_TREE))
        self.assertFalse(repaint_overlay.result_contains_repaint_rects('ABCD'))

    def test_extract_layer_tree(self):
        self.assertEquals(LAYER_TREE,
                          repaint_overlay.extract_layer_tree(LAYER_TREE))

    def test_generate_repaint_overlay_html(self):
        test_name = 'paint/invalidation/repaint-overlay/layers.html'
        host = Host()
        port = host.port_factory.get()
        layer_tree_file = port.expected_filename(test_name, '.txt')
        if not layer_tree_file or not host.filesystem.exists(layer_tree_file):
            # This can happen if the scripts are not in the standard blink directory.
            return

        layer_tree = str(host.filesystem.read_text_file(layer_tree_file))
        self.assertTrue(
            repaint_overlay.result_contains_repaint_rects(layer_tree))
        overlay_html = (
            '<!-- Generated by third_party/blink/tools/run_blinkpy_tests.py\n'
            +
            ' test case: TestRepaintOverlay.test_generate_repaint_overlay_html. -->\n'
            + repaint_overlay.generate_repaint_overlay_html(
                test_name, layer_tree, layer_tree))

        results_directory = port.results_directory()
        host.filesystem.maybe_make_directory(results_directory)
        actual_overlay_html_file = host.filesystem.join(
            results_directory, 'layers-overlay.html')
        host.filesystem.write_text_file(actual_overlay_html_file, overlay_html)

        overlay_html_file = port.abspath_for_test(
            'paint/invalidation/repaint-overlay/layers-overlay.html')
        expected = host.filesystem.read_text_file(overlay_html_file)

        self.assertEquals(
            expected, overlay_html,
            'This failure is probably caused by changed repaint_overlay.py. '
            'Please examine the diffs:\n  diff %s %s\n'
            'If the diffs are valid, update the file:\n  cp %s %s\n'
            'then update layers-overlay-expected.html in the same directory if needed,'
            ' and commit the files together with the changed repaint_overlay.py.' % \
            (overlay_html_file, actual_overlay_html_file, actual_overlay_html_file, overlay_html_file))
