""" Unit tests for ``wheezy.templates.console``.
"""

import unittest

from wheezy.template.console import main


class ConsoleTestCase(unittest.TestCase):
    """Test the console ``main`` function."""

    def test_usage(self) -> None:
        assert 2 == main(["-h"])
        assert 2 == main(["-t @"])
        assert 2 == main(["-j \\"])
        assert 2 == main(["-x"])

    def test_context_file(self) -> None:
        assert 0 == main(
            ["demos/helloworld/hello.txt", "demos/helloworld/hello.json"]
        )

    def test_context_string(self) -> None:
        assert 0 == main(["demos/helloworld/hello.txt", '{"name": "World"}'])

    def test_master(self) -> None:
        assert 0 == main(["-s", "demos/master", "index.html"])

    def test_line_join(self) -> None:
        assert 0 == main(["-j", "\\", "-s", "demos/master", "index.html"])
