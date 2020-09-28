# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A utility function to do text diffs of expected and actual web test results."""

import difflib


def unified_diff(expected_text, actual_text, expected_filename,
                 actual_filename):
    """Returns a string containing the diff of the two text strings
    in 'unified diff' format.
    """
    # The filenames show up in the diff output, make sure they're
    # raw bytes and not unicode, so that they don't trigger join()
    # trying to decode the input.
    expected_filename = _to_raw_bytes(expected_filename)
    actual_filename = _to_raw_bytes(actual_filename)

    diff = difflib.unified_diff(
        expected_text.splitlines(True), actual_text.splitlines(True),
        expected_filename, actual_filename)

    return ''.join(_diff_fixup(diff))


def _to_raw_bytes(string_value):
    if isinstance(string_value, unicode):
        return string_value.encode('utf-8')
    return string_value


def _diff_fixup(diff):
    # The diff generated by the difflib is incorrect if one of the files
    # does not have a newline at the end of the file and it is present in
    # the diff. Relevant Python issue: http://bugs.python.org/issue2142
    for line in diff:
        yield line
        if not line.endswith('\n'):
            yield '\n\\ No newline at end of file\n'
