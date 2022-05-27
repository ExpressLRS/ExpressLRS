import re
import typing

from external.wheezy.template.utils import find_balanced

RE_ARGS = re.compile(r'\s*(?P<expr>(([\'"]).*?\3|.+?))\s*\,')
RE_KWARGS = re.compile(
    r'\s*(?P<name>\w+)\s*=\s*(?P<expr>([\'"].*?[\'"]|.+?))\s*\,'
)
RE_STR_VALUE = re.compile(r'^[\'"](?P<value>.+)[\'"]$')
RE_INT_VALUE = re.compile(r"^(?P<value>(\d+))$")


# region: core extension


class DeterminedExtension(object):
    """Tranlates function calls between template engines.

    Strictly determined known calls are converted to preprocessor
    calls, e.g.::

        @_('Name:')
        @path_for('default')
        @path_for('static', path='/static/css/site.css')

    Those that are not strictly determined are ignored and processed
    by runtime engine.
    """

    def __init__(
        self,
        known_calls: typing.List[str],
        runtime_token_start: str = "@",
        token_start: str = "#",
    ) -> None:
        self.token_start = token_start
        self.pattern = re.compile(
            r"%s(%s)(?=\()" % (runtime_token_start, "|".join(known_calls))
        )
        self.preprocessors = [self.preprocess]

    def preprocess(self, source: str) -> str:
        result = []
        start = 0
        for m in self.pattern.finditer(source):
            pstart = m.end()
            pend = find_balanced(source, pstart)
            if determined(source[pstart + 1 : pend - 1]):
                name = m.group(1)
                result.append(source[start : m.start()])
                result.append(self.token_start + "ctx['" + name + "']")
                start = pstart
        if start:
            result.append(source[start:])
            return "".join(result)
        else:
            return source


def determined(expression: str) -> bool:
    """Checks if expresion is strictly determined.

    >>> determined("'default'")
    True
    >>> determined('name')
    False
    >>> determined("'default', id=id")
    False
    >>> determined("'default', lang=100")
    True
    >>> determined('')
    True
    """
    args, kwargs = parse_params(expression)
    for arg in args:
        if not str_or_int(arg):
            return False
    for arg in kwargs.values():
        if not str_or_int(arg):
            return False
    return True


def parse_kwargs(text: str) -> typing.Mapping[str, str]:
    """Parses key-value type of parameters.

    >>> parse_kwargs('id=item.id')
    {'id': 'item.id'}
    >>> sorted(parse_kwargs('lang="en", id=12').items())
    [('id', '12'), ('lang', '"en"')]
    """
    kwargs = {}
    for m in RE_KWARGS.finditer(text + ","):
        groups = m.groupdict()
        kwargs[groups["name"].rstrip("_")] = groups["expr"]
    return kwargs


def parse_args(text: str) -> typing.List[str]:
    """Parses argument type of parameters.

    >>> parse_args('')
    []
    >>> parse_args('10, "x"')
    ['10', '"x"']
    >>> parse_args("'x', 100")
    ["'x'", '100']
    >>> parse_args('"default"')
    ['"default"']
    """
    args = []
    for m in RE_ARGS.finditer(text + ","):
        args.append(m.group("expr"))
    return args


def parse_params(
    text: str,
) -> typing.Tuple[typing.List[str], typing.Mapping[str, str]]:
    """Parses function parameters.

    >>> parse_params('')
    ([], {})
    >>> parse_params('id=item.id')
    ([], {'id': 'item.id'})
    >>> parse_params('"default"')
    (['"default"'], {})
    >>> parse_params('"default", lang="en"')
    (['"default"'], {'lang': '"en"'})
    """
    if "=" in text:
        args = text.split("=")[0]
        if "," in args:
            args = args.rsplit(",", 1)[0]
            kwargs = text[len(args) :]
            return parse_args(args), parse_kwargs(kwargs)
        else:
            return [], parse_kwargs(text)
    else:
        return parse_args(text), {}


def str_or_int(text: str) -> bool:
    """Ensures ``text`` as string or int expression.

    >>> str_or_int('"default"')
    True
    >>> str_or_int("'Hello'")
    True
    >>> str_or_int('100')
    True
    >>> str_or_int('item.id')
    False
    """
    m = RE_STR_VALUE.match(text)
    if m:
        return True
    else:
        m = RE_INT_VALUE.match(text)
        if m:
            return True
    return False
