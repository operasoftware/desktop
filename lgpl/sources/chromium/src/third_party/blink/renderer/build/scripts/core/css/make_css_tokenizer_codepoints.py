#!/usr/bin/env python

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import in_generator

module_basename = os.path.basename(__file__)
module_pyname = os.path.splitext(module_basename)[0] + '.py'

CPP_TEMPLATE = """
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Auto-generated by {module_pyname}

const CSSTokenizer::CodePoint CSSTokenizer::kCodePoints[{array_size}] = {{
{token_lines}
}};
const unsigned codePointsNumber = {array_size};
"""


def token_type(i):
    codepoints = {
        '(': 'LeftParenthesis',
        ')': 'RightParenthesis',
        '[': 'LeftBracket',
        ']': 'RightBracket',
        '{': 'LeftBrace',
        '}': 'RightBrace',
        '+': 'PlusOrFullStop',
        '.': 'PlusOrFullStop',
        '-': 'HyphenMinus',
        '*': 'Asterisk',
        '<': 'LessThan',
        ',': 'Comma',
        '/': 'Solidus',
        '\\': 'ReverseSolidus',
        ':': 'Colon',
        ';': 'SemiColon',
        '#': 'Hash',
        '^': 'CircumflexAccent',
        '$': 'DollarSign',
        '|': 'VerticalLine',
        '~': 'Tilde',
        '@': 'CommercialAt',
        'u': 'LetterU',
        'U': 'LetterU',
    }
    c = chr(i)
    if c in codepoints:
        return codepoints[c]
    whitespace = '\n\r\t\f '
    quotes = '"\''
    if c in whitespace:
        return 'WhiteSpace'
    if c.isdigit():
        return 'AsciiDigit'
    if c.isalpha() or c == '_':
        return 'NameStart'
    if c in quotes:
        return 'StringStart'
    if i == 0:
        return 'EndOfFile'


class MakeCSSTokenizerCodePointsWriter(in_generator.Writer):
    def __init__(self, in_file_path):
        super(MakeCSSTokenizerCodePointsWriter, self).__init__(in_file_path)

        self._outputs = {
            ('css_tokenizer_codepoints.cc'): self.generate,
        }

    def generate(self):
        array_size = 128  # SCHAR_MAX + 1
        token_lines = [
            '    &CSSTokenizer::%s,' % token_type(i)
            if token_type(i) else '    0,' for i in range(array_size)
        ]
        return CPP_TEMPLATE.format(
            array_size=array_size,
            token_lines='\n'.join(token_lines),
            module_pyname=module_pyname)


if __name__ == '__main__':
    in_generator.Maker(MakeCSSTokenizerCodePointsWriter).main(sys.argv)
