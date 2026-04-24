import re
import typing

from external.wheezy.template.typing import Builder, LexerRule, ParserRule, Token
from external.wheezy.template.utils import find_balanced

# region: lexer extensions


def code_token(m: typing.Match[str]) -> Token:
    source = m.string
    start = m.end()
    end = find_balanced(source, start)
    if source[end::1] == "\n":
        end += 1
    return end, "code", source[start:end]


# region: parser


def parse_code(value: str) -> typing.List[str]:
    lines = value.rstrip("\n")[1:-1].split("\n")
    lines[0] = lines[0].lstrip()
    if len(lines) == 1:
        return lines
    line = lines[1]
    n = len(line) - len(line.lstrip())
    return [s[:n].lstrip() + s[n:] for s in lines]


# region: block_builders


def build_code(
    builder: Builder, lineno: int, token: str, lines: typing.List[str]
) -> bool:
    for line in lines:
        builder.add(lineno, line)
        lineno += 1
    return True


# region: core extension


class CodeExtension:
    """Includes support for embedded python code."""

    def __init__(self, token_start: str = "@") -> None:

        self.lexer_rules: typing.Mapping[int, LexerRule] = {
            300: (re.compile(r"\s*%s(?=\()" % token_start), code_token),
        }

    parser_rules: typing.Mapping[str, ParserRule] = {"code": parse_code}

    builder_rules: typing.List[typing.Tuple[str, typing.Any]] = [
        ("code", build_code)
    ]
