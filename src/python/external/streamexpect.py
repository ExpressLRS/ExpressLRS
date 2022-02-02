# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Copyright (c) 2015 Digi International Inc. All Rights Reserved.

# Downloaded from https://github.com/digidotcom/python-streamexpect

import collections
import re
import six
import socket
import sys
import time
import unicodedata


__version__ = '0.2.1'


class SequenceMatch(object):
    """Information about a match that has a concept of ordering."""

    def __init__(self, searcher, match, start, end):
        """
        :param Searcher searcher: The :class:`Searcher` that found the match
        :param match: Portion of sequence that triggered the match
        :param int start: Index of start of match
        :param int end: Index of item directly after match
        """
        self.searcher = searcher
        self.match = match
        self.start = int(start)
        self.end = int(end)

    def __repr__(self):
        return '{}({!r}, match={!r}, start={}, end={})'.format(
            self.__class__.__name__, self.searcher, self.match, self.start,
            self.end)


class RegexMatch(SequenceMatch):
    """Information about a match from a regex."""

    def __init__(self, searcher, match, start, end, groups):
        """
        :param Searcher searcher: The :class:`Searcher` that found the match
        :param match: Portion of sequence that triggered the match
        :param int start: Index of start of match
        :param int end: Index of item directly after match
        :param tuple groups: Contains the matched subgroups if the regex
            contained groups, otherwise ``None``
        """
        super(RegexMatch, self).__init__(searcher, match, start, end)
        self.groups = groups

    def __repr__(self):
        return '{}({!r}, match={!r}, start={}), end={}, groups={!r}'.format(
            self.__class__.__name__, self.searcher, self.match, self.start,
            self.end, self.groups)


class ExpectTimeout(Exception):
    """Exception raised when *expect* call exceeds a timeout."""


class Searcher(object):
    """Base class for searching buffers.

    Implements the base class for *Searcher* types, which are used by the
    library to determine whether or not a particular buffer contains a *match*.
    The type of the match is determined by the *Searcher* implementation: it
    may be bytes, text, or something else entirely.

    To conform to the *Searcher* interface, a class must implement one method
    *search* and one read-only property *match_type*. The buffer passed to
    the *search* method must match the type returned by the *match_type*
    property, and *search* must raise a `TypeError` if it does not. The
    member function :func:`_check_type` exists to provide this functionality
    for subclass implementations.
    """

    def __repr__(self):
        return '{}()'.format(self.__class__.__name__)

    def search(self, buf):
        """Search the provided buffer for a *match*.

        Search the provided buffer for a *match*. What exactly a *match* means
        is defined by the *Searcher* implementation. If the *match* is found,
        returns an `SequenceMatch` object, otherwise returns ``None``.

        :param buf: Buffer to search for a match.
        """
        raise NotImplementedError('search function must be provided')

    @property
    def match_type(self):
        """Read-only property that returns type matched by this *Searcher*"""
        raise NotImplementedError('match_type must be provided')

    def _check_type(self, value):
        """Checks that *value* matches the type of this *Searcher*.

        Checks that *value* matches the type of this *Searcher*, returning the
        value if it does and raising a `TypeError` if it does not.

        :return: *value* if type of *value* matches type of this *Searcher*.
        :raises TypeError: if type of *value* does not match the type of this
            *Searcher*
        """
        if not isinstance(value, self.match_type):
            raise TypeError('Type ' + str(type(value)) + ' does not match '
                            'expected type ' + str(self.match_type))
        else:
            return value


class BytesSearcher(Searcher):
    """Binary/ASCII searcher.

    A binary/ASCII searcher. Matches when the pattern passed to the
    constructor is found in the input buffer.

    Note that this class only operates on binary types. That means that in
    Python 3, it will fail on strings, as strings are Unicode by default. In
    Python 2 this class will fail on the Unicode type, as strings are ASCII by
    default.
    """

    def __init__(self, b):
        """
        :param b: Bytes to search for. Must be a binary type (i.e. bytes)
        """
        self._bytes = self._check_type(b)

    def __repr__(self):
        return '{}({!r})'.format(self.__class__.__name__, self._bytes)

    @property
    def match_type(self):
        return six.binary_type

    def search(self, buf):
        """Search the provided buffer for matching bytes.

        Search the provided buffer for matching bytes. If the *match* is found,
        returns a :class:`SequenceMatch` object, otherwise returns ``None``.

        :param buf: Buffer to search for a match.
        :return: :class:`SequenceMatch` if matched, None if no match was found.
        """
        idx = self._check_type(buf).find(self._bytes)
        if idx < 0:
            return None
        else:
            start = idx
            end = idx + len(self._bytes)
            return SequenceMatch(self, buf[start:end], start, end)


class TextSearcher(Searcher):
    """Plain text searcher.

    A plain-text searcher. Matches when the text passed to the constructor is
    found in the input buffer.

    Note that this class operates only on text types (i.e. Unicode) and raises
    a TypeError if used with binary data. Use the :class:`BytesSearcher` type
    to search binary or ASCII text.

    To make sure that modified (accented, grave, etc.) characters are matched
    accurately, the input text is converted to the Unicode canonical composed
    form before being used to match.
    """

    FORM = 'NFKC'

    def __init__(self, text):
        """
        :param text: Text to search for. Must be a text type (i.e. Unicode)
        """
        super(TextSearcher, self).__init__()
        self._check_type(text)
        self._text = unicodedata.normalize(self.FORM, text)

    def __repr__(self):
        return '{}({!r})'.format(self.__class__.__name__, self._text)

    @property
    def match_type(self):
        return six.text_type

    def search(self, buf):
        """Search the provided buffer for matching text.

        Search the provided buffer for matching text. If the *match* is found,
        returns a :class:`SequenceMatch` object, otherwise returns ``None``.

        :param buf: Buffer to search for a match.
        :return: :class:`SequenceMatch` if matched, None if no match was found.
        """
        self._check_type(buf)
        normalized = unicodedata.normalize(self.FORM, buf)
        idx = normalized.find(self._text)
        if idx < 0:
            return None
        start = idx
        end = idx + len(self._text)
        return SequenceMatch(self, normalized[start:end], start, end)


class RegexSearcher(Searcher):
    """Regular expression searcher.

    Searches for a match in the stream that matches the provided regular
    expression.

    This class follows the Python 3 model for dealing with binary versus text
    patterns, raising a `TypeError` if mixed binary/text is used. This means
    that a *RegexSearcher* that is instantiated with binary data will raise a
    `TypeError` if used on text, and a *RegexSearcher* instantiated with text
    will raise a `TypeError` on binary data.
    """

    def __init__(self, pattern, regex_options=0):
        """
        :param pattern: The regex to search for, as a single compiled regex
            or a string that will be processed as a regex.
        :param regex_options: Options passed to the regex engine.
        """
        super(RegexSearcher, self).__init__()
        self._regex = re.compile(pattern, regex_options)

    def __repr__(self):
        return '{}(re.compile({!r}))'.format(self.__class__.__name__,
                                             self._regex.pattern)

    @property
    def match_type(self):
        return type(self._regex.pattern)

    def search(self, buf):
        """Search the provided buffer for a match to the object's regex.

        Search the provided buffer for a match to the object's regex. If the
        *match* is found, returns a :class:`RegexMatch` object, otherwise
        returns ``None``.

        :param buf: Buffer to search for a match.
        :return: :class:`RegexMatch` if matched, None if no match was found.
        """
        match = self._regex.search(self._check_type(buf))
        if match is not None:
            start = match.start()
            end = match.end()
            return RegexMatch(self, buf[start:end], start, end, match.groups())


def _flatten(n):
    """Recursively flatten a mixed sequence of sub-sequences and items"""
    if isinstance(n, collections.Sequence):
        for x in n:
            for y in _flatten(x):
                yield y
    else:
        yield n


class SearcherCollection(Searcher, list):
    """Collect multiple `Searcher` objects into one.

    Collect multiple `Searcher` instances into a single `Searcher` instance.
    This is different than simply looping over a list of searchers, as this
    class will always find the earliest match from any of its sub-searchers
    (i.e. the match with the smallest index).

    Note that this class requires that all of its sub-searchers have the same
    *match_type*.
    """

    def __init__(self, *searchers):
        """
        :param searchers: One or more :class:`Searcher` implementations.
        """
        super(SearcherCollection, self).__init__()
        self.extend(_flatten(searchers))
        if not self:
            raise ValueError(self.__class__.__name__ + ' requires at least '
                             'one sub-searcher to be specified')

        # Check that all searchers are valid
        for searcher in self:
            try:
                getattr(searcher, 'search')
            except AttributeError:
                raise TypeError('missing required attribute "search"')
            try:
                getattr(searcher, 'match_type')
            except AttributeError:
                raise TypeError('missing required attribute "match_type"')

        # Check that all searchers are the same match type
        match_type = self[0].match_type
        if not all(map(lambda x: x.match_type == match_type, self)):
            raise ValueError(self.__class__.__name__ + ' requires that all '
                             'sub-searchers implement the same match_type')
        self._match_type = match_type

    def __repr__(self):
        return '{}({!r})'.format(self.__class__.__name__, list(self))

    @property
    def match_type(self):
        return self._match_type

    def search(self, buf):
        """Search the provided buffer for a match to any sub-searchers.

        Search the provided buffer for a match to any of this collection's
        sub-searchers. If a single matching sub-searcher is found, returns that
        sub-searcher's *match* object. If multiple matches are found, the match
        with the smallest index is returned. If no matches are found, returns
        ``None``.

        :param buf: Buffer to search for a match.
        :return: :class:`RegexMatch` if matched, None if no match was found.
        """
        self._check_type(buf)
        best_match = None
        best_index = sys.maxsize
        for searcher in self:
            match = searcher.search(buf)
            if match and match.start < best_index:
                best_match = match
                best_index = match.start
        return best_match


class StreamAdapter(object):
    """Adapter to match varying stream objects to a single interface.

    Despite the existence of the Python stream interface and file-like objects,
    there are actually a number of subtly different implementations of streams
    within Python. In addition, there are stream-like constructs like sockets
    that use a different interface entirely (*send*/*recv* versus
    *read*/*write*).

    This class provides a base adapter that can be used to convert anything
    even remotely stream-like into a form that can consistently be used by
    implementations of `Expecter`. The key method is :func:`poll`, which must
    *always* provide a blocking interface to the underlying stream, and must
    *also* provide a reliable timeout mechanism. The exact method to achieve
    these two goals is implementation dependent, and a particular
    implementation may be used to meet the need at hand.

    This class also automatically delegates any non-existent attributes to the
    underlying stream object. This allows the adapter to be used identically to
    the stream.
    """
    def __init__(self, stream):
        """:param stream: Stream object to wrap over."""
        self.stream = stream

    def __getattr__(self, attr):
        return getattr(self.stream, attr)

    def __repr__(self):
        return '{}({!r})'.format(self.__class__.__name__, self.stream)

    def poll(self, timeout):
        """Unified blocking read access to the underlying stream.

        All subclasses of :class:`StreamAdapter` must implement this method.
        Once called, the method must either:

          - Return new read data whenever it becomes available, or
          - Raise an `ExpectTimeout` exception if timeout is exceeded.

        The amount of data to return from each call is implementation
        dependent, but it is important that either all data is returned from
        the function, or that the data be somehow returned to the stream. In
        other words, any data not returned must still be available the next
        time the `poll` method is called.

        Note that there is no "wait forever" functionality: either some new
        data must be returned or an exception must occur in a finite amount of
        time. It is also important that, if there is a timeout, the method
        raise the exception as soon after the timeout occurred as is reasonably
        possible.
        """
        raise NotImplementedError(self.__class__.__name__ +
                                  '.poll must be implemented')


class PollingStreamAdapterMixin(object):
    """Add *poll_period* and *max_read* properties to a `StreamAdapter`"""

    @property
    def poll_period(self):
        return self._poll_period

    @poll_period.setter
    def poll_period(self, value):
        value = float(value)
        if value <= 0:
            raise ValueError('poll_period must be greater than 0')
        self._poll_period = value

    @property
    def max_read(self):
        return self._max_read

    @max_read.setter
    def max_read(self, value):
        value = int(value)
        if value < 0:
            raise ValueError('max_read must be greater than or equal to 0')
        self._max_read = value


class PollingStreamAdapter(StreamAdapter, PollingStreamAdapterMixin):
    """A :class:`StreamAdapter` that polls a non-blocking stream.

    Polls a non-blocking stream of data until new data is available or a
    timeout is exceeded. It is *VERY IMPORTANT* that the underlying stream be
    non-blocking.
    """

    def __init__(self, stream, poll_period=0.1, max_read=1024):
        """
        :param stream: Stream to poll for data.
        :param float poll_period: Time (in seconds) between polls of the
            stream.
        :param int max_read: The maximum number of bytes/characters to read
            from the stream at one time.
        """
        super(PollingStreamAdapter, self).__init__(stream)
        self.poll_period = poll_period
        self.max_read = max_read

    def poll(self, timeout):
        """
        :param float timeout: Timeout in seconds.
        """
        timeout = float(timeout)
        end_time = time.time() + timeout
        while True:
            # Keep reading until data is received or timeout
            incoming = self.stream.read(self._max_read)
            if incoming:
                return incoming
            if (end_time - time.time()) < 0:
                raise ExpectTimeout()
            time.sleep(self._poll_period)


class PollingSocketStreamAdapter(StreamAdapter, PollingStreamAdapterMixin):
    """A :class:`StreamAdapter` that polls a non-blocking socket.

    Polls a non-blocking socket for data until new data is available or a
    timeout is exceeded.
    """

    def __init__(self, sock, poll_period=0.1, max_read=1024):
        """
        :param sock: Socket to poll for data.
        :param float poll_period: Time (in seconds) between poll of the socket.
        :param int max_read: The maximum number of bytes/characters to read
            from the socket at one time.
        """
        super(PollingSocketStreamAdapter, self).__init__(sock)
        self.poll_period = poll_period
        self.max_read = max_read

    def poll(self, timeout):
        """
        :param float timeout: Timeout in seconds. A timeout that is less than
            the poll_period will still cause a single read that may take up to
            poll_period seconds.
        """
        now = time.time()
        end_time = now + float(timeout)
        prev_timeout = self.stream.gettimeout()
        self.stream.settimeout(self._poll_period)
        incoming = None
        try:
            while (end_time - now) >= 0:
                try:
                    incoming = self.stream.recv(self._max_read)
                except socket.timeout:
                    pass
                if incoming:
                    return incoming
                now = time.time()
            raise ExpectTimeout()
        finally:
            self.stream.settimeout(prev_timeout)


class ExpectBytesMixin(object):

    def expect_bytes(self, b, timeout=3):
        """Wait for a match to the bytes in *b* to appear on the stream.

        Waits for input matching the bytes *b* for up to *timeout* seconds.
        If a match is found, a :class:`SequenceMatch` result is returned.  If
        no match is found within *timeout* seconds, raise an
        :class:`ExpectTimeout` exception.

        :param b: The byte pattern to search for.
        :param float timeout: Timeout in seconds.
        :return: :class:`SequenceMatch` if matched, None if no match was found.
        """
        return self.expect(BytesSearcher(b), timeout)


class ExpectTextMixin(object):

    def expect_text(self, text, timeout=3):
        """Wait for a match to the text in *text* to appear on the stream.

        Waits for input matching the text *text* for up to *timeout*
        seconds. If a match is found, a :class:`SequenceMatch` result is
        returned.  If no match is found within *timeout* seconds, raise an
        :class:`ExpectTimeout` exception.

        :param text: The plain-text pattern to search for.
        :param float timeout: Timeout in seconds.
        :return: :class:`SequenceMatch` if matched, None if no match was found.
        """
        return self.expect(TextSearcher(text), timeout)


class ExpectRegexMixin(object):

    def expect_regex(self, pattern, timeout=3, regex_options=0):
        """Wait for a match to the regex in *pattern* to appear on the stream.

        Waits for input matching the regex *pattern* for up to *timeout*
        seconds. If a match is found, a :class:`RegexMatch` result is returned.
        If no match is found within *timeout* seconds, raise an
        :class:`ExpectTimeout` exception.

        :param pattern: The pattern to search for, as a single compiled regex
            or a string that will be processed as a regex.
        :param float timeout: Timeout in seconds.
        :param regex_options: Options passed to the regex engine.
        :return: :class:`RegexMatch` if matched, None if no match was found.
        """
        return self.expect(RegexSearcher(pattern, regex_options), timeout)


class Expecter(object):
    """Base class for consuming input and waiting for a pattern to appear.

    Implements the base class for *Expecter* types, which wrap over a
    :class:`StreamAdapter` type and provide methods for applying a
    :class:`Searcher` to the received data. Any attributes not part of this
    class are delegated to the underlying :class:`StreamAdapter` type.
    """

    def __init__(self, stream_adapter, input_callback, window, close_adapter):
        """
        :param StreamAdapter stream_adapter: The :class:`StreamAdapter` object
            to receive data from.
        :param function input_callback: Callback function with one parameter
            that is called each time new data is read from the
            *stream_adapter*.
        :param int window: Number of historical objects (bytes, characters,
            etc.) to buffer.
        :param bool close_adapter: If ``True``, and the Expecter is used as a
            context manager, closes the adapter at the end of the context
            manager.
        """
        self.stream_adapter = stream_adapter
        if not input_callback:
            self.input_callback = lambda _: None
        else:
            self.input_callback = input_callback
        self.window = window
        self.close_adapter = close_adapter

    # Delegate undefined methods to underlying stream
    def __getattr__(self, attr):
        return getattr(self._stream_adapter, attr)

    def __enter__(self):
        return self

    def __exit__(self, type_, value, traceback):
        if self.close_adapter:
            self._stream_adapter.close()
        return False

    @property
    def stream_adapter(self):
        return self._stream_adapter

    @stream_adapter.setter
    def stream_adapter(self, value):
        try:
            getattr(value, 'poll')
        except AttributeError:
            raise TypeError('stream_adapter must define "poll" method')
        self._stream_adapter = value

    @property
    def window(self):
        return self._window

    @window.setter
    def window(self, value):
        value = int(value)
        if value < 1:
            raise ValueError('window must be at least 1')
        self._window = value

    def expect(self, searcher, timeout):
        """Apply *searcher* to underlying :class:`StreamAdapter`

        :param Searcher searcher: :class:`Searcher` to apply to underlying
            stream.
        :param float timeout: Timeout in seconds.
        """
        raise NotImplementedError('Expecter must implement "expect"')


class BytesExpecter(Expecter, ExpectBytesMixin, ExpectRegexMixin):
    """:class:`Expecter` interface for searching a byte-oriented stream."""

    def __init__(self, stream_adapter, input_callback=None, window=1024,
                 close_adapter=True):
        """
        :param StreamAdapter stream_adapter: The :class:`StreamAdapter` object
            to receive data from.
        :param function input_callback: Callback function with one parameter
            that is called each time new data is read from the
            *stream_adapter*.
        :param int window: Number of historical bytes to buffer.
        """
        super(BytesExpecter, self).__init__(stream_adapter, input_callback,
                                            window, close_adapter)
        self._history = six.binary_type()
        self._start = 0

    def expect(self, searcher, timeout=3):
        """Wait for input matching *searcher*

        Waits for input matching *searcher* for up to *timeout* seconds. If
        a match is found, the match result is returned (the specific type of
        returned result depends on the :class:`Searcher` type). If no match is
        found within *timeout* seconds, raise an :class:`ExpectTimeout`
        exception.

        :param Searcher searcher: :class:`Searcher` to apply to underlying
            stream.
        :param float timeout: Timeout in seconds.
        """
        timeout = float(timeout)
        end = time.time() + timeout
        match = searcher.search(self._history[self._start:])
        while not match:
            # poll() will raise ExpectTimeout if time is exceeded
            incoming = self._stream_adapter.poll(end - time.time())
            self.input_callback(incoming)
            self._history += incoming
            match = searcher.search(self._history[self._start:])
            trimlength = len(self._history) - self._window
            if trimlength > 0:
                self._start -= trimlength
                self._history = self._history[trimlength:]

        self._start += match.end
        if (self._start < 0):
            self._start = 0

        return match


class TextExpecter(Expecter, ExpectTextMixin, ExpectRegexMixin):
    """:class:`Expecter` interface for searching a text-oriented stream."""

    def __init__(self, stream_adapter, input_callback=None, window=1024,
                 close_adapter=True):
        """
        :param StreamAdapter stream_adapter: The :class:`StreamAdapter` object
            to receive data from.
        :param function input_callback: Callback function with one parameter
            that is called each time new data is read from the
            *stream_adapter*.
        :param int window: Number of historical characters to buffer.
        """
        super(TextExpecter, self).__init__(stream_adapter, input_callback,
                                           window, close_adapter)
        self._history = six.text_type()
        self._start = 0

    def expect(self, searcher, timeout=3):
        """Wait for input matching *searcher*.

        Waits for input matching *searcher* for up to *timeout* seconds. If
        a match is found, the match result is returned (the specific type of
        returned result depends on the :class:`Searcher` type). If no match is
        found within *timeout* seconds, raise an :class:`ExpectTimeout`
        exception.

        :param Searcher searcher: :class:`Searcher` to apply to underlying
            stream.
        :param float timeout: Timeout in seconds.
        """
        timeout = float(timeout)
        end = time.time() + timeout
        match = searcher.search(self._history[self._start:])
        while not match:
            # poll() will raise ExpectTimeout if time is exceeded
            incoming = self._stream_adapter.poll(end - time.time())
            self.input_callback(incoming)
            self._history += incoming
            match = searcher.search(self._history[self._start:])
            trimlength = len(self._history) - self._window
            if trimlength > 0:
                self._start -= trimlength
                self._history = self._history[trimlength:]

        self._start += match.end
        if (self._start < 0):
            self._start = 0

        return match


def _echo_text(value):
    sys.stdout.write(value)


def _echo_bytes(value):
    sys.stdout.write(value.decode('ascii', errors='backslashreplace'))


def wrap(stream, unicode=False, window=1024, echo=False, close_stream=True):
    """Wrap a stream to implement expect functionality.

    This function provides a convenient way to wrap any Python stream (a
    file-like object) or socket with an appropriate :class:`Expecter` class for
    the stream type. The returned object adds an :func:`Expect.expect` method
    to the stream, while passing normal stream functions like *read*/*recv*
    and *write*/*send* through to the underlying stream.

    Here's an example of opening and wrapping a pair of network sockets::

        import socket
        import streamexpect

        source, drain = socket.socketpair()
        expecter = streamexpect.wrap(drain)
        source.sendall(b'this is a test')
        match = expecter.expect_bytes(b'test', timeout=5)

        assert match is not None

    :param stream: The stream/socket to wrap.
    :param bool unicode: If ``True``, the wrapper will be configured for
        Unicode matching, otherwise matching will be done on binary.
    :param int window: Historical characters to buffer.
    :param bool echo: If ``True``, echoes received characters to stdout.
    :param bool close_stream: If ``True``, and the wrapper is used as a context
        manager, closes the stream at the end of the context manager.
    """
    if hasattr(stream, 'read'):
        proxy = PollingStreamAdapter(stream)
    elif hasattr(stream, 'recv'):
        proxy = PollingSocketStreamAdapter(stream)
    else:
        raise TypeError('stream must have either read or recv method')

    if echo and unicode:
        callback = _echo_text
    elif echo and not unicode:
        callback = _echo_bytes
    else:
        callback = None

    if unicode:
        expecter = TextExpecter(proxy, input_callback=callback, window=window,
                                close_adapter=close_stream)
    else:
        expecter = BytesExpecter(proxy, input_callback=callback, window=window,
                                 close_adapter=close_stream)

    return expecter


__all__ = [
    # Functions
    'wrap',

    # Expecter types
    'Expecter',
    'BytesExpecter',
    'TextExpecter',

    # Searcher types
    'Searcher',
    'BytesSearcher',
    'TextSearcher',
    'RegexSearcher',
    'SearcherCollection',

    # Match types
    'SequenceMatch',
    'RegexMatch',

    # StreamAdapter types
    'StreamAdapter',
    'PollingStreamAdapter',
    'PollingSocketStreamAdapter',
    'PollingStreamAdapterMixin',

    # Exceptions
    'ExpectTimeout',
]
