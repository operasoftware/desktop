#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Lint as: python3
"""Tests for upload_download_utils.

To Execute:
  PYTHONPATH=chrome/test chrome/test/data/android/upload_download_utils_test.py
"""
import multiprocessing
import os
import sys
import unittest
import upload_download_utils

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..', '..'))
PYMOCK_PATH = os.path.join(
    CHROMIUM_SRC, 'third_party', 'pymock')
sys.path.append(PYMOCK_PATH)
import mock

from pyfakefs import fake_filesystem_unittest


def filter_all(f):
  """Filter everything through."""
  return True

def filter_image_only(f):
  """Filter only png file through."""
  if f.endswith('png'):
    return True
  return False

class UploadDownloadUtilsTest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    """Set up the fake file system."""
    self.setUpPyfakefs()

  def test_get_files_to_delete(self):
    pathname = os.path.join(THIS_DIR, 'tmp')
    self.fs.create_dir(pathname)
    self.fs.create_file(os.path.join(pathname, 'image1.png'))
    self.fs.create_file(os.path.join(pathname, 'image1.sha1'))
    self.fs.create_file(os.path.join(pathname, 'image2.png'))
    self.fs.create_file(os.path.join(pathname, 'image3.png'))
    self.fs.create_file(os.path.join(pathname, 'image3.png.sha1'))
    self.fs.create_file(os.path.join(pathname, 'scenario1.wprgo'))
    self.fs.create_file(os.path.join(pathname, 'scenario1.wprgo.sha1'))

    expected_file_list = [
        os.path.join(pathname, 'image1.png'),
        os.path.join(pathname, 'image1.sha1'),
        os.path.join(pathname, 'image2.png'),
        os.path.join(pathname, 'image3.png.sha1'),
        os.path.join(pathname, 'scenario1.wprgo.sha1'),
    ].sort()
    self.assertEquals(
        upload_download_utils._get_files_to_delete(pathname, filter_all).sort(),
        expected_file_list)

    expected_file_list = [
        os.path.join(pathname, 'image1.png'),
        os.path.join(pathname, 'image2.png'),
    ].sort()
    self.assertEquals(
        upload_download_utils._get_files_to_delete(pathname, filter_image_only)
            .sort(),
        expected_file_list)

  def test_get_files_to_upload_with_entries(self):
    pathname = os.path.join(THIS_DIR, 'tmp')
    self.fs.create_dir(pathname)
    self.fs.create_file(os.path.join(pathname, 'image1.png'))
    self.fs.create_file(os.path.join(pathname, 'image1.png.sha1'))
    self.fs.create_file(os.path.join(pathname, 'image2.png'))
    self.fs.create_file(os.path.join(pathname, 'scenario1.wprgo'))
    expected_file_list = [
        os.path.join(pathname, 'image1.png'),
        os.path.join(pathname, 'image1.png.sha1'),
        os.path.join(pathname, 'image2.png'),
        os.path.join(pathname, 'scenario1.wprgo'),
    ].sort()
    self.assertEquals(
        upload_download_utils._get_files_to_upload(pathname, filter_all).sort(),
        expected_file_list)

    expected_file_list = [
        os.path.join(pathname, 'image1.png'),
        os.path.join(pathname, 'image2.png'),
    ].sort()
    self.assertEquals(
        upload_download_utils._get_files_to_upload(pathname, filter_image_only)
            .sort(),
        expected_file_list)

  def test_get_files_to_upload_no_entries(self):
    pathname = os.path.join(THIS_DIR, 'tmp')
    self.fs.create_dir(pathname)
    self.fs.create_file(os.path.join(pathname, 'scenario1.wprgo'))
    self.fs.create_file(os.path.join(pathname, 'scenario1.wprgo.sha1'))
    self.fs.create_file(os.path.join(pathname, 'scenario2.wprgo'))
    self.fs.create_file(os.path.join(pathname, 'scenario3.wprgo'))
    self.assertEquals(
        upload_download_utils._get_files_to_upload(pathname, filter_image_only),
        [])

  @mock.patch.object(multiprocessing, 'cpu_count')
  def test_get_thread_count_not_implemented(self, multiplecore_mock):
    multiplecore_mock.side_effect = NotImplementedError('abc')
    self.assertEqual(upload_download_utils._get_thread_count(), 4)

  @mock.patch.object(multiprocessing, 'cpu_count')
  def test_get_thread_count_with_quad_core(self, multiplecore_mock):
    multiplecore_mock.return_value = 4
    self.assertEqual(upload_download_utils._get_thread_count(), 4)

  @mock.patch.object(multiprocessing, 'cpu_count')
  def test_get_thread_count_with_hexa_core(self, multiplecore_mock):
    multiplecore_mock.return_value = 6
    self.assertEqual(upload_download_utils._get_thread_count(), 12)


if __name__ == '__main__':
  unittest.main(verbosity=2)

