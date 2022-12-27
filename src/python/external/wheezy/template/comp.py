import ast
import typing
from _thread import allocate_lock  # noqa


def adjust_source_lineno(source: str, name: str, lineno: int) -> typing.Any:
    node = compile(source, name, "exec", ast.PyCF_ONLY_AST)
    ast.increment_lineno(node, lineno)
    return node
