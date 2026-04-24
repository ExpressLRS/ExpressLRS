import typing

from external.wheezy.template.typing import (
    LexerRule,
    PostProcessorRule,
    PreProcessorRule,
    Token,
)


def lexer_scan(
    extensions: typing.List[typing.Any],
) -> typing.Mapping[str, typing.Any]:
    """Scans extensions for ``lexer_rules`` and ``preprocessors``
    attributes.
    """
    lexer_rules: typing.Dict[int, LexerRule] = {}
    preprocessors: typing.List[PreProcessorRule] = []
    postprocessors: typing.List[PostProcessorRule] = []
    for extension in extensions:
        if hasattr(extension, "lexer_rules"):
            lexer_rules.update(extension.lexer_rules)
        if hasattr(extension, "preprocessors"):
            preprocessors.extend(extension.preprocessors)
        if hasattr(extension, "postprocessors"):
            postprocessors.extend(extension.postprocessors)
    return {
        "lexer_rules": [lexer_rules[k] for k in sorted(lexer_rules.keys())],
        "preprocessors": preprocessors,
        "postprocessors": postprocessors,
    }


class Lexer(object):
    """Tokenizes input source per rules supplied."""

    def __init__(
        self,
        lexer_rules: typing.List[LexerRule],
        preprocessors: typing.Optional[typing.List[PreProcessorRule]] = None,
        postprocessors: typing.Optional[typing.List[PostProcessorRule]] = None,
        **ignore: typing.Any
    ) -> None:
        """Initializes with ``rules``. Rules must be a list of
        two elements tuple: ``(regex, tokenizer)`` where
        tokenizer if a callable of the following contract::

        def tokenizer(match):
            return end_index, token, value
        """
        self.rules = lexer_rules
        self.preprocessors = preprocessors or []
        self.postprocessors = postprocessors or []

    def tokenize(self, source: str) -> typing.List[Token]:
        """Translates ``source`` accoring to lexer rules into
        an iteratable of tokens.
        """
        for preprocessor in self.preprocessors:
            source = preprocessor(source)
        tokens: typing.List[Token] = []
        append = tokens.append
        pos = 0
        lineno = 1
        end = len(source)
        while pos < end:
            for regex, tokenizer in self.rules:
                m = regex.match(source, pos, end)
                if m is not None:
                    npos, token, value = tokenizer(m)
                    assert npos > pos
                    append((lineno, token, value))
                    lineno += source[pos:npos].count("\n")
                    pos = npos
                    break
            else:
                raise AssertionError("Lexer pattern mismatch.")
        for postprocessor in self.postprocessors:
            postprocessor(tokens)
        return tokens
