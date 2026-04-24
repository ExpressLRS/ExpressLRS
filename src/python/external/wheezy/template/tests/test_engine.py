import typing
import unittest

from wheezy.template.engine import Engine
from wheezy.template.ext.core import CoreExtension
from wheezy.template.loader import DictLoader


class EngineTestCase(unittest.TestCase):
    """Test the ``Engine``."""

    def setUp(self) -> None:
        self.templates: typing.Dict[str, str] = {"a": ""}
        self.engine = Engine(
            loader=DictLoader(templates=self.templates),
            extensions=[CoreExtension()],
        )

    def test_template_not_found(self) -> None:
        """Raises IOError."""
        self.assertRaises(IOError, lambda: self.engine.get_template("x"))

    def test_import_not_found(self) -> None:
        """Raises IOError."""
        self.assertRaises(IOError, lambda: self.engine.import_name("x"))

    def test_remove_unknown_name(self) -> None:
        """Invalidate name that is not known to engine."""
        self.engine.remove("x")

    def test_remove_name(self) -> None:
        """Invalidate name that is known to engine."""
        # self.templates["a"] = ""
        self.engine.compile_import("a")
        self.engine.compile_template("a")
        self.engine.remove("a")


class EngineSyntaxErrorTestCase(unittest.TestCase):
    """Test the ``Engine``."""

    def setUp(self) -> None:
        self.templates: typing.Dict[str, str] = {}
        self.engine = Engine(
            loader=DictLoader(templates=self.templates),
            extensions=[CoreExtension()],
        )

    def test_compile_template_error(self) -> None:
        """Raises SyntaxError."""
        self.templates[
            "x"
        ] = """
            @if :
            @end
        """
        self.assertRaises(
            SyntaxError, lambda: self.engine.compile_template("x")
        )

    def test_compile_import_error(self) -> None:
        """Raises SyntaxError."""
        self.templates[
            "m"
        ] = """
            @def x():
                @# ignore
                @if :
                @end
            @end
        """
        self.assertRaises(SyntaxError, lambda: self.engine.compile_import("m"))
