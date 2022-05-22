import getopt
import json
import os
import sys
import typing

from external.wheezy.template.engine import Engine
from external.wheezy.template.ext.code import CodeExtension
from external.wheezy.template.ext.core import CoreExtension
from external.wheezy.template.loader import FileLoader

try:
    from wheezy.html.utils import escape_html as escape  # type: ignore[import]
except ImportError:  # pragma: nocover
    from html import escape


class Options:
    def __init__(
        self,
        token_start: str = "@",
        line_join: str = "\\",
        searchpath: typing.Optional[typing.List[str]] = None,
        extensions: typing.Optional[typing.List[str]] = None,
    ) -> None:
        self.token_start = token_start
        self.line_join = line_join
        self.searchpath = searchpath or ["."]
        self.extensions = extensions or []
        self.template = ""
        self.context: typing.List[str] = []


def main(argv: typing.Optional[typing.List[str]] = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    if not args:
        return 2
    ts = args.token_start
    extensions = [CoreExtension(ts, args.line_join), CodeExtension(ts)]
    extensions.extend(args.extensions)
    engine = Engine(FileLoader(args.searchpath), extensions)
    engine.global_vars.update({"h": escape})
    t = engine.get_template(args.template)
    sys.stdout.write(t.render(load_context(args.context)))
    return 0


def load_context(sources: typing.List[str]) -> typing.Mapping[str, typing.Any]:
    c: typing.Dict[str, typing.Any] = {}
    for s in sources:
        if os.path.isfile(s):
            d = json.load(open(s))
        else:
            d = json.loads(s)
        c.update(d)
    return c


def parse_args(  # noqa: C901
    args: typing.List[str],
) -> typing.Optional[Options]:
    try:
        opts, value = getopt.getopt(args, "s:t:j:wh")
    except getopt.GetoptError:
        e = sys.exc_info()[1]
        usage()
        print("error: %s" % e)
        return None
    d = Options(token_start="@", searchpath=["."], extensions=[])
    for o, a in opts:
        if o == "-h":
            return None
        elif o == "-t":
            d.token_start = a
        elif o == "-j":
            d.line_join = a
        elif o == "-s":
            d.searchpath = a.split(";")
        elif o == "-w":  # pragma: nocover
            from wheezy.html.ext.template import (  # type: ignore[import]
                WhitespaceExtension,
            )

            d.extensions.append(WhitespaceExtension())
    if not value:
        usage()
        return None
    d.template = value[0]
    d.context = value[1:]
    return d


def usage() -> None:
    from datetime import datetime
    from os.path import basename

    from wheezy.template import __version__

    print(
        """\
wheezy.template %s
Copyright (C) 2012-%d by Andriy Kornatskyy

renders a template with the given context.

usage: %s template [ context ... ]

positional arguments:

  template    a filename
  context     a filename or JSON string

optional arguments:

  -s path     search path for templates ( . )
  -t token    token start ( @ )
  -j token    line join ( \\ )
  -w          whitespace clean up
  -h          show this help message
"""
        % (__version__, datetime.now().year, basename(sys.argv[0]))
    )


if __name__ == "__main__":  # pragma: nocover
    sys.exit(main())
