import unittest

from wheezy.template.parser import Parser


class ParserTestCase(unittest.TestCase):
    """Test the ``Parser``."""

    def setUp(self) -> None:
        self.parser = Parser({})
        self.tokens = [
            (1, "a", "11"),
            (2, "b", "12"),
            (3, "c", "13"),
            (4, "b", "14"),
            (5, "c", "15"),
        ]

    def test_end_continue(self) -> None:
        """Ensure end nodes are inserted before continue tokens."""
        self.parser.continue_tokens = ["b"]
        nodes = list(self.parser.end_continue(self.tokens))
        assert 7 == len(nodes)
        assert (2, "end", "") == nodes[1]
        assert (2, "b", "12") == nodes[2]
        assert (4, "end", "") == nodes[4]
        assert (4, "b", "14") == nodes[5]

    def test_parse_unchanged(self) -> None:
        """If there is no parser tokens defined the result is unchanged
        input.
        """
        nodes = list(self.parser.parse(self.tokens))
        assert self.tokens == nodes

    def test_parse_with_rules(self) -> None:
        """Ensure the rules applied."""
        self.parser.rules["a"] = lambda value: value + "100"
        self.parser.rules["b"] = lambda value: value + "10"
        nodes = list(self.parser.parse(self.tokens))
        assert (1, "a", "11100") == nodes[0]
        assert (2, "b", "1210") == nodes[1]
        assert (4, "b", "1410") == nodes[3]

    def test_out_tokens(self) -> None:
        """Tokens from ``out_tokens`` are combined together into a single
        node.
        """
        self.parser.out_tokens = ["a", "b"]
        nodes = list(self.parser.parse(self.tokens))
        assert 4 == len(nodes)
        assert (1, "out", [(1, "a", "11"), (2, "b", "12")]) == nodes[0]
        assert (4, "out", [(4, "b", "14")]) == nodes[2]

        self.parser.out_tokens = ["b", "c"]
        nodes = list(self.parser.parse(self.tokens))
        assert 2 == len(nodes)
        assert (
            2,
            "out",
            [(2, "b", "12"), (3, "c", "13"), (4, "b", "14"), (5, "c", "15")],
        ) == nodes[1]

    def test_compound(self) -> None:
        """"""
        self.parser.compound_tokens = ["b"]
        self.parser.end_tokens = ["c"]
        nodes = list(self.parser.parse(self.tokens))
        assert 3 == len(nodes)
        assert (2, "b", ("12", [(3, "c", "13")])) == nodes[1]
        assert (4, "b", ("14", [(5, "c", "15")])) == nodes[2]
