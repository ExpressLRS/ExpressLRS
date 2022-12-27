import typing
from abc import abstractmethod

Token = typing.Tuple[int, str, str]


class Builder:
    lineno: int

    @abstractmethod
    def start_block(self) -> None:
        ...  # pragma: nocover

    @abstractmethod
    def end_block(self) -> None:
        ...  # pragma: nocover

    @abstractmethod
    def add(self, lineno: int, code: str) -> None:
        ...  # pragma: nocover

    @abstractmethod
    def build_block(self, nodes: typing.Iterable[Token]) -> None:
        ...  # pragma: nocover

    @abstractmethod
    def build_token(
        self,
        lineno: int,
        token: str,
        value: typing.Union[str, typing.Iterable[Token]],
    ) -> None:
        ...  # pragma: nocover


Tokenizer = typing.Callable[[typing.Match], Token]
LexerRule = typing.Tuple[typing.Pattern, Tokenizer]
PreProcessorRule = typing.Callable[[str], str]
PostProcessorRule = typing.Callable[[typing.List[Token]], str]
BuilderRule = typing.Callable[
    [
        Builder,
        int,
        str,
        typing.Union[str, typing.List[str], typing.Iterable[Token]],
    ],
    bool,
]
ParserRule = typing.Callable[[str], typing.Union[str, typing.List[str]]]


class ParserConfig:
    end_tokens: typing.List[str]
    continue_tokens: typing.List[str]
    compound_tokens: typing.List[str]
    out_tokens: typing.List[str]


RenderTemplate = typing.Callable[
    [
        typing.Mapping[str, typing.Any],
        typing.Mapping[str, typing.Any],
        typing.Mapping[str, typing.Any],
    ],
    str,
]


class SupportsRender:
    @abstractmethod
    def render(self, ctx: typing.Mapping[str, typing.Any]) -> str:
        ...  # pragma: nocover


TemplateClass = typing.Callable[[str, RenderTemplate], SupportsRender]


class Loader:
    @abstractmethod
    def list_names(self) -> typing.Tuple[str, ...]:
        ...  # pragma: nocover

    @abstractmethod
    def load(self, name: str) -> typing.Optional[str]:
        ...  # pragma: nocover
