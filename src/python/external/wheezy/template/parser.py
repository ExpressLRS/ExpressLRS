import typing

from external.wheezy.template.typing import ParserConfig, ParserRule, Token


def parser_scan(
    extensions: typing.List[typing.Any],
) -> typing.Mapping[str, typing.Any]:
    parser_rules = {}
    parser_configs = []
    for extension in extensions:
        if hasattr(extension, "parser_rules"):
            parser_rules.update(extension.parser_rules)
        if hasattr(extension, "parser_configs"):
            parser_configs.extend(extension.parser_configs)
    return {
        "parser_rules": parser_rules,
        "parser_configs": parser_configs,
    }


class Parser(ParserConfig):
    """
    ``continue_tokens`` are used to insert ``end`` node right
    before them to simulate a block end. Such nodes have token
    value ``None``.

    ``out_tokens`` are combined together into a single node.
    """

    def __init__(
        self,
        parser_rules: typing.Dict[str, ParserRule],
        parser_configs: typing.Optional[
            typing.List[typing.Callable[[ParserConfig], None]]
        ] = None,
        **ignore: typing.Any
    ) -> None:
        self.end_tokens: typing.List[str] = []
        self.continue_tokens: typing.List[str] = []
        self.compound_tokens: typing.List[str] = []
        self.out_tokens: typing.List[str] = []
        self.rules = parser_rules
        if parser_configs:
            for config in parser_configs:
                config(self)

    def end_continue(
        self, tokens: typing.List[Token]
    ) -> typing.Iterator[Token]:
        """If token is in ``continue_tokens`` prepend it
        with end token so it simulate a closed block.
        """
        for t in tokens:
            if t[1] in self.continue_tokens:
                yield (t[0], "end", "")
            yield t

    def parse_iter(
        self, tokens: typing.Iterator[Token]
    ) -> typing.Iterator[typing.Any]:
        operands = []
        for lineno, token, value in tokens:
            if token in self.rules:
                value = self.rules[token](value)  # type: ignore[assignment]
            if token in self.out_tokens:
                operands.append((lineno, token, value))
            else:
                if operands:
                    yield operands[0][0], "out", operands
                    operands = []
                if token in self.compound_tokens:
                    yield lineno, token, (value, list(self.parse_iter(tokens)))
                else:
                    yield lineno, token, value
                    if token in self.end_tokens:
                        break
        if operands:
            yield operands[0][0], "out", operands

    def parse(self, tokens: typing.List[Token]) -> typing.List[typing.Any]:
        return list(self.parse_iter(self.end_continue(tokens)))
