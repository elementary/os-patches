# -*- coding: utf-8 -*-
# Onboard is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Onboard is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Copyright © 2012, marmuta
#
# This file is part of Onboard.

from __future__ import division, print_function, unicode_literals

import time

### Logging ###
import logging
_logger = logging.getLogger("TextChanges")
###############


class TextSpan:
    """
    Span of text

    Doctests:
    >>> span = TextSpan(3, 2, "0123456789")
    >>> span.get_span_text()
    '34'
    >>> span.get_text_until_span()
    '01234'
    >>> span.get_text_from_span()
    '3456789'
    """

    def __init__(self, pos = 0, length = 0, text = "", text_pos = 0):
        self.pos = pos              # document caret position
        self.length = length        # span length
        self.text = text            # text that includes span, but may be larger
        self.text_pos = text_pos    # document position of text begin
        self.last_modified = None

    def copy(self):
        return TextSpan(self.pos, self.length, self.text, self.text_pos)

    def begin(self):
        return self.pos

    def end(self):
        return self.pos + self.length

    def text_begin(self):
        return self.text_pos

    def is_empty(self):
        return self.length == 0

    def contains(self, pos):
        return self.pos <= pos < self.pos + self.length

    def intersects(self, span):
        return not self.intersection(span).is_empty()

    def intersection(self, span):
       p0 = max(self.pos, span.pos)
       p1 = min(self.pos + self.length,  span.pos + span.length)
       if p0 > p1:
           return TextSpan()
       else:
           return TextSpan(p0, p1 - p0)

    def union_inplace(self, span):
        """
        Join two spans, result in self.

        Doctests:
        - adjacent spans
        >>> a = TextSpan(2, 3, "0123456789")
        >>> b = TextSpan(5, 2, "0123456789")
        >>> a.union_inplace(b)                         # doctest: +ELLIPSIS
        TextSpan(2, 5, '23456', ...
        >>> a.get_text()
        '0123456789'

        - intersecting spans
        >>> a = TextSpan(2, 3, "0123456789")
        >>> b = TextSpan(4, 2, "0123456789")
        >>> a.union_inplace(b)                         # doctest: +ELLIPSIS
        TextSpan(2, 4, '2345', ...
        >>> a.get_text()
        '0123456789'
        """
        begin = min(self.begin(), span.begin())
        end   = max(self.end(),   span.end())
        length = end - begin
        middle = length // 2
        self.text   = self.text[:middle - self.text_pos] + \
                      span.text[middle - span.text_pos:]
        self.pos    = begin
        self.length = length
        self.last_modified = max(self.last_modified if self.last_modified else 0,
                                 span.last_modified if span.last_modified else 0)
        return self

    def get_text(self, begin = None, end = None):
        """ Return the whole available text """
        if begin is None and end is None:
            return self.text

        if begin is None:
            begin = self.pos
        if end is None:
            end = self.end()

        return self.text[begin - self.text_pos : end - self.text_pos]

    def get_span_text(self):
        """ Return just the span's part of the text. """
        return self.get_text(self.pos, self.end())

    def get_text_until_span(self):
        """
        Return the beginning of the whole available text,
        ending with and including the span.
        """
        return self.text[:self.end() - self.text_pos]

    def get_text_from_span(self):
        """
        Return the end of the whole available text,
        starting from and including the span.
        """
        return self.text[self.pos - self.text_pos:]

    def get_char_before_span(self):
        """
        Character right before the span.

        Doctests:
        >>> span = TextSpan(0, 0, "0123456789", 0)
        >>> span.get_char_before_span()
        ''

        >>> span = TextSpan(9, 1, "0123456789", 0)
        >>> span.get_char_before_span()
        '8'

        >>> span = TextSpan(5, 2, "3456789", 3)
        >>> span.get_char_before_span()
        '4'
        """
        pos = self.pos - self.text_pos
        return self.text[pos - 1 : pos]

    def _escape(self, text):
        return text.replace("\n", "\\n")

    def __repr__(self):
        return "TextSpan({}, {}, '{}', {}, {})" \
                .format(self.pos, self.length,
                        self._escape(self.get_span_text()),
                        self.text_begin(),
                        self.last_modified)


class TextChanges:
    __doc__ = """
    Collection of text spans yet to be learned.

    Example:
    >>> c = TextChanges()
    >>> c.insert(0, 1) # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 1]]

    Doctests:
    # insert and extend span
    >>> c = TextChanges()
    >>> c.insert(0, 1) # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 1]]
    >>> c.insert(0, 1) # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 2]]

    # extend at beginning and end
    >>> c = TextChanges()
    >>> c.insert(0, 1); c.insert(1, 1); c.insert(0, 3) # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 5]]

    # insert separated by at least one character -> multiple spans
    >>> c = TextChanges()
    >>> c.insert(1, 1); c.insert(0, 1) # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 1], [2, 1]]

    # add and delete inside single span
    >>> c = TextChanges()
    >>> c.insert(0, 9); # IGNORE_RESULT
    >>> c.delete(2, 1); # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 8]]

    # join spans when deleting
    >>> c = TextChanges()
    >>> c.insert(0, 1); c.insert(2, 1) # IGNORE_RESULT
    >>> c.delete(2, 1);                # IGNORE_RESULT
    >>> c.delete(1, 1);                # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 1]]

    # remove spans fully contained in the deleted range
    >>> c = TextChanges()
    >>> c.insert(2, 1); c.insert(4, 1) # IGNORE_RESULT
    >>> c.delete(0, 5);                # IGNORE_RESULT
    >>> c.get_span_ranges()
    [[0, 0]]

    # partially delete span, with and without recording empty spans
    #             ins     del     res with          res without
    >>> tests = [ # deletion before span
    ...          [[2, 3], [0, 5], [[0, 0]],         [[0, 0]] ],
    ...          [[3, 3], [0, 5], [[0, 1]],         [[0, 1]] ],
    ...          [[4, 3], [0, 5], [[0, 2]],         [[0, 2]] ],
    ...          [[5, 3], [0, 5], [[0, 3]],         [[0, 3]] ],
    ...          [[6, 3], [0, 5], [[0, 0], [1, 3]], [[1, 3]] ],
    ...           # deletion after span
    ...          [[0, 3], [4, 5], [[0, 3], [4, 0]], [[0, 3]] ],
    ...          [[1, 3], [4, 5], [[1, 3]],         [[1, 3]] ],
    ...          [[2, 3], [4, 5], [[2, 2]],         [[2, 2]] ],
    ...          [[3, 3], [4, 5], [[3, 1]],         [[3, 1]] ],
    ...           # deletion completely inside of span
    ...          [[4, 3], [4, 5], [[4, 0]],         [[4, 0]] ],
    ...          [[0, 9], [2, 3], [[0, 6]],         [[0, 6]] ] ]
    >>> for test in tests:
    ...     c = TextChanges()
    ...     _ = c.insert(*test[0]); _ = c.delete(test[1][0], test[1][1], True)
    ...     if c.get_span_ranges() != test[2]:
    ...        "test1: " + repr(test) + " result: " + repr(c.get_span_ranges())
    ...     c = TextChanges()
    ...     _ = c.insert(*test[0]); _ = c.delete(test[1][0], test[1][1], False)
    ...     if c.get_span_ranges() != test[3]:
    ...        "test2: " + repr(test) + " result: " + repr(c.get_span_ranges())

    # insert excluded span, include_length=0 to always insert an empty span
    #             ins     del     result
    >>> tests = [[[5, 5], [2, 3], [[2, 0], [8, 5]] ],  # insert before span
    ...          [[0, 5], [6, 3], [[0, 5], [6, 0]] ],  # insert after span
    ...          [[0, 5], [2, 3], [[0, 2], [5, 3]] ],  # insert inside span
    ...          [[0, 5], [3, 4], [[0, 3], [7, 2]] ] ] # insert at span end
    >>> for test in tests:
    ...     c = TextChanges()
    ...     _= c.insert(*test[0]); _ = c.insert(test[1][0], test[1][1], 0)
    ...     if c.get_span_ranges() != test[2]:
    ...        "test: " + repr(test) + " result: " + repr(c.get_span_ranges())

    """.replace('IGNORE_RESULT', 'doctest: +ELLIPSIS\n    [...')

    def __init__(self, spans = None):
        self.clear()
        if spans:
            self._spans = spans

    def clear(self):
        self._spans = []

        # some counts for book-keeping, not used by this class itself.
        self.insert_count = 0
        self.delete_count = 0

    def is_empty(self):
        return len(self._spans) == 0

    def get_spans(self):
        return self._spans

    def remove_span(self, span):
        self._spans.remove(span)

    def get_change_count(self):
        return self.insert_count + self.delete_count

    def insert(self, pos, length, include_length = -1):
        """
        Record insertion up to <include_length> characters,
        counted from the start of the insertion. The remaining
        inserted characters are excluded from spans. This may split
        an existing span.

        A small but non-zero <include_length> allows to skip over
        possible whitespace at the start of the insertion and
        will often result in including the very first word(s) for learning.

        include_length =   -1: include length
        include_length =   +n: include n
        include_length = None: include nothing, don't record
                               zero length span either
        """
        end = pos + length
        spans_to_update = []

        # shift all existing spans after position
        for span in self._spans:
            if span.pos > pos:
                span.pos += length
                spans_to_update.append(span)

        if include_length == -1:
            # include all of the insertion
            span = self.find_span_at(pos)
            if span:
                span.length += length
            else:
                span = TextSpan(pos, length);
                self._spans.append(span)
            spans_to_update.append(span)
        else:
            # include the insertion up to include_length only
            max_include = min(length, include_length or 0)
            span = self.find_span_at(pos)
            if span:
                 # cut existing span
                old_length = span.length
                span.length = pos - span.pos + max_include
                spans_to_update.append(span)

                # new span for the cut part
                l = old_length - span.length
                if l > 0 or \
                   l == 0 and include_length is None:
                    span2 = TextSpan(pos + length, l)
                    self._spans.append(span2)
                    spans_to_update.append(span2)

            elif not include_length is None:
                span = TextSpan(pos, max_include)
                self._spans.append(span)
                spans_to_update.append(span)

        t = time.time()
        for span in spans_to_update:
            span.last_modified = t

        if spans_to_update:
            self.insert_count += 1

        return spans_to_update

    def delete(self, pos, length, record_empty_spans = True):
        """
        Record deletion.

        record_empty_spans =  True: record extra zero length spans
                                    at deletion point
        record_empty_spans = False: no extra new spans, but keep existing ones
                                    that become zero length (terminal scrolling)
        """
        begin = pos
        end   = pos + length
        spans_to_update = []

        #from pudb import set_trace; set_trace()

        # cut/remove existing spans
        for span in list(self._spans):
            if span.pos <= pos:          # span begins before deletion point?
                k = min(span.end() - begin, length)   # intersecting length
                if k >= 0:
                    span.length -= k
                    spans_to_update.append(span)
            else:                        # span begins after deletion point
                k = end - span.begin()   # intersecting length
                if k >= 0:
                    span.pos += k
                    span.length -= k
                span.pos -= length       # shift by deleted length

                # remove spans fully contained in the deleted range
                if span.length < 0:
                    self._spans.remove(span)
                else:
                    spans_to_update.append(span)

        # Add new empty span
        if record_empty_spans:
            span = self.find_span_excluding(pos)
            if not span:
                # Create empty span when deleting too, because this
                # is still a change that can result in a word to learn.
                span = TextSpan(pos, 0);
                self._spans.append(span)

            self._spans, span = self.consolidate_spans(self._spans, span)
            spans_to_update.append(span)

        if spans_to_update:
            self.delete_count += 1

        return spans_to_update

    @staticmethod
    def consolidate_spans(spans, tracked_span = None):
        """
        join touching or intersecting text spans

        Doctests:
        # Join touching spans
        >>> spans = [TextSpan(0, 1),
        ...          TextSpan(2, 4),
        ...          TextSpan(1, 1),
        ...          TextSpan(10, 3),
        ...          TextSpan(8, 2)]
        >>> spans, _span = TextChanges.consolidate_spans(spans)
        >>> TextChanges.to_span_ranges(spans)
        [[0, 6], [8, 5]]

        # Join overlapping spans
        >>> spans = [TextSpan(2, 5),
        ...          TextSpan(4, 10),
        ...          TextSpan(12, 8)]
        >>> spans, _span = TextChanges.consolidate_spans(spans)
        >>> TextChanges.to_span_ranges(spans)
        [[2, 18]]

        # Join contained spans
        >>> spans = [TextSpan(5, 1),
        ...          TextSpan(2, 10),
        ...          TextSpan(3, 4)]
        >>> spans, _span = TextChanges.consolidate_spans(spans)
        >>> TextChanges.to_span_ranges(spans)
        [[2, 10]]
        """
        spans = sorted(spans, key=lambda x: (x.begin(), x.end()))
        new_spans = []
        slast = None
        for s in spans:
            if slast and \
               slast.end() >= s.begin():
                slast.union_inplace(s)
                if tracked_span is s:
                    tracked_span = slast
            else:
                new_spans.append(s)
                slast = s

        return new_spans, tracked_span

    def find_span_at(self, pos):
        """
        Doctests:
        - find empty spans (text deleted):
        >>> c = TextChanges()
        >>> c.insert(0, 0)      # doctest: +ELLIPSIS
        [TextSpan(...
        >>> c.find_span_at(0)   # doctest: +ELLIPSIS
        TextSpan(0, 0,...
        """
        for span in self._spans:
            if span.pos <= pos <= span.pos + span.length:
                return span
        return None

    def find_span_excluding(self, pos):
        """
        Doctests:
        - find empty spans (text deleted):
        >>> c = TextChanges()
        >>> c.insert(0, 0)             # doctest: +ELLIPSIS
        [TextSpan(...
        >>> c.find_span_excluding(0)   # doctest: +ELLIPSIS
        TextSpan(0, 0,...

        - don't match the end
        >>> c = TextChanges()
        >>> c.insert(0, 1)      # doctest: +ELLIPSIS
        [TextSpan(...
        >>> c.find_span_excluding(1)   # doctest: +ELLIPSIS

        """
        for span in self._spans:
            if span.pos == pos or \
               span.pos <= pos < span.pos + span.length:
                return span
        return None

    def get_span_ranges(self):
        return self.to_span_ranges(self._spans)

    @staticmethod
    def to_span_ranges(spans):
        return sorted([[span.pos, span.length] for span in spans])

    def __repr__(self):
        return "TextChanges " + repr([str(span) for span in self._spans])

