import typing
import unittest

from wheezy.template.engine import Engine
from wheezy.template.ext.core import CoreExtension
from wheezy.template.loader import DictLoader
from wheezy.template.preprocessor import Preprocessor
from wheezy.template.typing import Loader


class PreprocessorTestCase(unittest.TestCase):
    """Test the ``Preprocessor``."""

    def setUp(self) -> None:
        def runtime_engine_factory(loader: Loader) -> Engine:
            engine = Engine(
                loader=loader,
                extensions=[
                    CoreExtension(),
                ],
            )
            return engine

        self.templates: typing.Dict[str, str] = {}
        engine = Engine(
            loader=DictLoader(templates=self.templates),
            extensions=[
                CoreExtension("#", line_join=""),
            ],
        )
        self.engine = Preprocessor(
            runtime_engine_factory, engine, key_factory=lambda ctx: ""
        )

    def render(self, name: str, ctx: typing.Mapping[str, typing.Any]) -> str:
        template = self.engine.get_template(name)
        return template.render(ctx)

    def test_render(self) -> None:
        self.templates[
            "test.html"
        ] = """\
#require(_)
@require(username)
#_('Welcome,') @username!"""

        assert "Welcome, John!" == self.render(
            "test.html", ctx={"_": lambda x: x, "username": "John"}
        )

    def test_extends(self) -> None:
        self.templates.update(
            {
                "master.html": """\
#require(_)
@def say_hi(name):
    #_('Hello,') @name!
@end
@say_hi('John')""",
                "tmpl.html": """\
#require(_)
@extends('master.html')
@def say_hi(name):
    #_('Hi,') @name!
@end
""",
            }
        )

        assert "    Hi, John!\n" == self.render(
            "tmpl.html",
            ctx={
                "_": lambda x: x,
            },
        )

    def test_remove(self) -> None:
        self.templates["test.html"] = "Hello"
        assert "Hello" == self.render("test.html", {})
        self.engine.remove("x")
