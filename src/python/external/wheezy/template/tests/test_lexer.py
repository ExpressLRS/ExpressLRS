import re
import typing
import unittest

from wheezy.template.lexer import Lexer, lexer_scan
from wheezy.template.typing import Token


class LexerTestCase(unittest.TestCase):
    """Test the ``Lexer``."""

    def test_tokenize(self) -> None:
        """Test with simple rules"""

        def word_token(m: typing.Match[str]) -> Token:
            return m.end(), "w", m.group()

        def blank_token(m: typing.Match[str]) -> Token:
            return m.end(), "b", m.group()

        def to_upper(s: str) -> str:
            return s.upper()

        def cleanup(tokens: typing.List[Token]) -> None:
            for i in range(len(tokens)):
                t = tokens[i]
                if t[i] == "b":
                    tokens[i] = (t[0], "b", " ")

        class Extension(object):
            lexer_rules = {
                100: (re.compile(r"\w+"), word_token),
                200: (re.compile(r"\s+"), blank_token),
            }
            preprocessors = [to_upper]
            postprocessors = [cleanup]

        lexer = Lexer(**lexer_scan([Extension]))
        assert [
            (1, "w", "HELLO"),
            (1, "b", " "),
            (2, "w", "WORLD"),
        ] == lexer.tokenize("hello\n world")

    def test_trivial(self) -> None:
        """Empty rules and source"""
        lexer = Lexer([])
        assert [] == lexer.tokenize("")

    def test_raises_error(self) -> None:
        """If there is no match it raises AssertionError."""
        lexer = Lexer([])
        self.assertRaises(AssertionError, lambda: lexer.tokenize("test"))
