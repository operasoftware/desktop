#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate list of attribute names known to this version of Chromium."""

from pyjson5.src import json5
import optparse
import sys


def error(context, *infos):
    """Print a brief error message and return an error code."""
    messages = ["An error occurred when when " + context + ":"]
    messages.extend(infos)
    print("\n\t".join(map(str, messages)))
    return 1


def lstrip(string):
    """Call str.lstrip on each line of text."""
    return "\n".join(map(str.lstrip, string.split("\n")))


def prolog(out):
    """Print the beginning of the source file."""
    print(lstrip("""
        // Copyright 2022 The Chromium Authors
        // Use of this source code is governed by a BSD-style license that can be
        // found in the LICENSE file.

        // This file is automatically generated. Do not edit. Just generate.
        // $ ninja -C ... generate_sanitizer_attribute_lists

        #include "third_party/blink/renderer/modules/sanitizer_api/builtins/sanitizer_builtins.h"
        """),
          file=out)


def namespace(out, name):
    if name:
        print("namespace %s {\n" % name, file=out)


def end_namespace(out, name):
    if name:
        print("}  // namespace %s\n" % name, file=out)


def string_list(out, name, items):
    """Print a list of strings as a C++ array of const char*."""
    print(f"const char* const {name}[] = {{", file=out)
    for item in sorted(items):
        print(f"  \"{item}\",", file=out)
    print("  nullptr,", file=out)
    print("};", file=out)
    print("", file=out)


def main(argv):
    parser = optparse.OptionParser()
    parser.add_option("--out")
    parser.add_option("--cxx-namespace")
    options, args = parser.parse_args(argv)
    if not options.out:
        parser.error("--out is required")

    names = set()
    try:
        for source in args:
            json = json5.load(open(source, "r"))
            names |= set(json["data"])
    except BaseException as err:
        return error("reading input file", source, err)

    try:
        with open(options.out, "w") as out:
            prolog(out)
            namespace(out, "blink")
            namespace(out, options.cxx_namespace)
            string_list(out, "kKnownAttributes", names)
            end_namespace(out, options.cxx_namespace)
            end_namespace(out, "blink")
    except BaseException as err:
        return error("writing output file", options.out, err)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
