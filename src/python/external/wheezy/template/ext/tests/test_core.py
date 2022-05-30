import typing
import unittest

from wheezy.template.engine import Engine
from wheezy.template.ext.core import CoreExtension, all_tokens
from wheezy.template.loader import DictLoader
from wheezy.template.typing import Token

hello = "\u043f\u0440\u0438\u0432\u0456\u0442"


class CleanSourceTestCase(unittest.TestCase):
    """Test the ``clean_source``."""

    def setUp(self) -> None:
        self.clean_source = CoreExtension().preprocessors[0]

    def test_new_line(self) -> None:
        """Replace windows new line with linux new line."""
        assert "a\nb" == self.clean_source("a\r\nb")

    def test_clean_leading_whitespace(self) -> None:
        """Remove leading whitespace before @<stmt>, e.g. @if, @for, etc."""
        for token in all_tokens:
            assert "@" + token == self.clean_source("  @" + token)
            assert "\n@" + token == self.clean_source("\n  @" + token)
            assert "a\n@" + token == self.clean_source("a\n  @" + token)

    def test_leave_leading_whitespace(self) -> None:
        """Leave leading whitespace before @<var> tokens."""
        assert "a\n\n   @b" == self.clean_source("a\n\n   @b")
        assert "a\n @b" == self.clean_source("a\n @b")
        assert "a\n@b" == self.clean_source("a\n@b")
        assert "a@b" == self.clean_source("a@b")
        assert "  @b" == self.clean_source("  @b")

    def test_ignore(self) -> None:
        """Ignore double @."""
        assert "a\n  @@b" == self.clean_source("a\n  @@b")
        assert "  @@b" == self.clean_source("  @@b")


class LexerTestCase(unittest.TestCase):
    """Test the ``CoreExtension`` lexers."""

    def setUp(self) -> None:
        self.engine = Engine(
            loader=DictLoader({}), extensions=[CoreExtension()]
        )

    def tokenize(self, source: str) -> typing.List[Token]:
        return self.engine.lexer.tokenize(source)

    def test_stmt_token(self) -> None:
        """Test statement token."""
        tokens = self.tokenize("@require(title, users)\n")
        assert (1, "require", "require(title, users)") == tokens[0]

    def test_comment_token(self) -> None:
        """Test statement token."""
        tokens = self.tokenize("@#ignore\\\n@end\n")
        assert (1, "#", "#ignore@end") == tokens[0]

    def test_line_join(self) -> None:
        """Test line join."""
        tokens = self.tokenize("a \\\nb")
        assert (1, "markup", "a \\\nb") == tokens[0]
        tokens = self.tokenize("a \\\\\nb")
        assert (1, "markup", "a \\\\\nb") == tokens[0]

    def test_var_token(self) -> None:
        """Test variable token."""
        tokens = self.tokenize("@user.name ")
        assert (1, "var", "user.name") == tokens[0]
        tokens = self.tokenize("@user.pref[i].fmt() ")
        assert (1, "var", "user.pref[i].fmt()") == tokens[0]
        tokens = self.tokenize('@f("()")')
        assert (1, "var", 'f("()")') == tokens[0]
        tokens = self.tokenize('@f("a@a!x")!h')
        assert (1, "var", 'f("a@a!x")!!h') == tokens[0]

    def test_var_token_unicode(self) -> None:
        """Test variable token with unicode string as argument."""
        t = '_("' + hello + '")'
        tokens = self.tokenize("@" + t)
        assert (1, "var", t) == tokens[0]

    def test_var_token_filter(self) -> None:
        """Test variable token filter."""
        tokens = self.tokenize("@user.age!s")
        assert (1, "var", "user.age!!s") == tokens[0]
        tokens = self.tokenize("@user.age!s!h")
        assert (1, "var", "user.age!!s!h") == tokens[0]
        # escape or ignore !
        tokens = self.tokenize("@user.age!s!")
        assert (1, "var", "user.age!!s") == tokens[0]
        assert (1, "markup", "!") == tokens[1]
        tokens = self.tokenize("@user.age!!s")
        assert (1, "var", "user.age") == tokens[0]
        assert (1, "markup", "!!s") == tokens[1]
        tokens = self.tokenize("@user! ")
        assert (1, "var", "user") == tokens[0]
        assert (1, "markup", "! ") == tokens[1]

    def test_rvalue_token(self) -> None:
        """Test rvalue token."""
        tokens = self.tokenize("@{user.name}")
        assert (1, "var", "user.name") == tokens[0]
        tokens = self.tokenize("@{ user.name }")
        assert (1, "var", "user.name") == tokens[0]
        tokens = self.tokenize("@{ s(user.age) }")
        assert (1, "var", "s(user.age)") == tokens[0]

    def test_rvalue_token_unicode(self) -> None:
        """Test rvalue token with unicode string as argument."""
        t = '_("' + hello + '")'
        tokens = self.tokenize("@{ " + t + " }")
        assert (1, "var", t) == tokens[0]

    def test_rvalue_token_filter(self) -> None:
        """Test rvalue token filter."""
        tokens = self.tokenize("@{ user.age!!s }")
        assert (1, "var", "user.age!!s") == tokens[0]
        tokens = self.tokenize("@{ user.age!!s!h }")
        assert (1, "var", "user.age!!s!h") == tokens[0]

    def test_markup_token(self) -> None:
        """Test markup token."""
        tokens = self.tokenize(" test ")
        assert 1 == len(tokens)
        assert (1, "markup", " test ") == tokens[0]
        tokens = self.tokenize("x@n")
        assert (1, "markup", "x") == tokens[0]

    def test_markup_token_escape(self) -> None:
        """Test markup token with escape."""
        tokens = self.tokenize("support@@acme.org")
        assert 1 == len(tokens)
        assert (1, "markup", "support@acme.org") == tokens[0]


class ParserTestCase(unittest.TestCase):
    """Test the ``CoreExtension`` parsers."""

    def setUp(self) -> None:
        self.engine = Engine(
            loader=DictLoader({}), extensions=[CoreExtension()]
        )

    def parse(self, source: str) -> typing.List[typing.Any]:
        return list(
            self.engine.parser.parse(self.engine.lexer.tokenize(source))
        )

    def test_require(self) -> None:
        """Test parse_require."""
        nodes = self.parse("@require(title, users)\n")
        assert [(1, "require", ["title", "users"])] == nodes

    def test_extends(self) -> None:
        """Test parse_extends."""
        nodes = self.parse('@extends("shared/master.html")\n')
        assert [(1, "extends", ('"shared/master.html"', []))] == nodes

    def test_include(self) -> None:
        """Test parse_include."""
        nodes = self.parse('@include("shared/scripts.html")\n')
        assert [(1, "out", [(1, "include", '"shared/scripts.html"')])] == nodes

    def test_markup(self) -> None:
        """Test parse_markup."""
        nodes = self.parse(
            """
 Welcome, @name!
"""
        )
        assert [
            (
                1,
                "out",
                [
                    (1, "markup", "'\\n Welcome, '"),
                    (2, "var", ("name", None)),
                    (2, "markup", "'!\\n'"),
                ],
            )
        ] == nodes
        nodes = self.parse("")
        assert [] == nodes

    def test_line_join(self) -> None:
        nodes = self.parse("a \\\nb")
        assert [(1, "out", [(1, "markup", "'a b'")])] == nodes
        nodes = self.parse("\\\n")
        assert [(1, "out", [(1, "markup", None)])] == nodes
        nodes = self.parse("a \\\\\nb")
        assert [(1, "out", [(1, "markup", "'a \\\\\\nb'")])] == nodes

    def test_var(self) -> None:
        """Test parse_markup."""
        nodes = self.parse("@name!h!")
        assert [
            (1, "out", [(1, "var", ("name", ["h"])), (1, "markup", "'!'")])
        ] == nodes
        assert nodes == self.parse("@{ name!!h }!")
        nodes = self.parse("@name!s!h!")
        assert [
            (
                1,
                "out",
                [(1, "var", ("name", ["s", "h"])), (1, "markup", "'!'")],
            )
        ] == nodes
        assert nodes == self.parse("@{ name!!s!h }!")
        nodes = self.parse("@user.pref[i].fmt() ")
        assert [
            (
                1,
                "out",
                [
                    (1, "var", ("user.pref[i].fmt()", None)),
                    (1, "markup", "' '"),
                ],
            )
        ] == nodes
        assert nodes == self.parse("@{ user.pref[i].fmt() } ")
        nodes = self.parse('@f("()")')
        assert [(1, "out", [(1, "var", ('f("()")', None))])] == nodes
        assert nodes == self.parse('@{ f("()") }')
        nodes = self.parse('@f("a@a!x")!h')
        assert [(1, "out", [(1, "var", ('f("a@a!x")', ["h"]))])] == nodes
        assert nodes == self.parse('@{ f("a@a!x")!!h }')


class ParserLineJoinTestCase(unittest.TestCase):
    """Test the ``CoreExtension`` parsers."""

    def setUp(self) -> None:
        self.engine = Engine(
            loader=DictLoader({}), extensions=[CoreExtension(line_join="")]
        )

    def parse(self, source: str) -> typing.List[typing.Any]:
        return list(
            self.engine.parser.parse(self.engine.lexer.tokenize(source))
        )

    def test_markup(self) -> None:
        """Test parse_markup."""
        nodes = self.parse("")
        assert [] == nodes
        assert not self.engine.parser.rules["markup"]("")

    def test_line_join(self) -> None:
        nodes = self.parse("a \\\nb")
        assert [(1, "out", [(1, "markup", "'a \\\\\\nb'")])] == nodes
        nodes = self.parse("a \\\\\nb")
        assert [(1, "out", [(1, "markup", "'a \\\\\\\\\\nb'")])] == nodes


class BuilderTestCase(unittest.TestCase):
    """Test the ``CoreExtension`` builders."""

    def setUp(self) -> None:
        self.engine = Engine(
            loader=DictLoader({}), extensions=[CoreExtension()]
        )

    def build_source(self, source: str) -> str:
        nodes = list(
            self.engine.parser.parse(self.engine.lexer.tokenize(source))
        )
        return self.engine.builder.build_source(nodes)

    def build_render(self, source: str) -> str:
        nodes = list(
            self.engine.parser.parse(self.engine.lexer.tokenize(source))
        )
        return self.engine.builder.build_render(nodes)

    def test_markup(self) -> None:
        assert "w('Hello')" == self.build_source("Hello")
        assert "" == self.build_source("")

    def test_line_join(self) -> None:
        assert "" == self.build_source("\\\n")
        assert "w('a b')" == self.build_source("a \\\nb")
        assert "w('a \\\\\\nb')" == self.build_source("a \\\\\nb")

    def test_comment(self) -> None:
        assert """\
w('Hello')
# comment
w(' World')""" == self.build_source(
            """\
Hello\\
@# comment
 World"""
        )

    def test_require(self) -> None:
        assert """\
title = ctx['title']; username = ctx['username']
w(username)""" == self.build_source(
            """\
@require(title, username)
@username"""
        )

    def test_out(self) -> None:
        """Test build_out."""
        expected = "w('Welcome, '); w(username); w('!')"
        assert expected == self.build_source("Welcome, @username!")
        assert expected == self.build_source("Welcome, @{ username }!")
        expected = """\
w('\\n<i>\\n    ')

w(username); w('\\n</i>')"""
        assert expected == self.build_source(
            """
<i>
    @username
</i>"""
        )
        assert expected == self.build_source(
            """
<i>
    @{ username }
</i>"""
        )

    def test_include(self) -> None:
        """Test build_include."""
        expected = 'w(_r("a", ctx, local_defs, super_defs))'
        assert expected == self.build_source('@include("a")')

    def test_if(self) -> None:
        """Test if elif else statements."""
        assert """\
if n > 0:
    w('    Positive\\n')
elif n == 0:
    w('    Zero\\n')
else:
    w('    Negative\\n')""" == self.build_source(
            """\
@if n > 0:
    Positive
@elif n == 0:
    Zero
@else:
    Negative
@end
"""
        )

    def test_for(self) -> None:
        assert """\
for color in colors:
    w('    '); w(color); w('\\n')""" == self.build_source(
            """\
@for color in colors:
    @color
@end
"""
        )

    def test_def(self) -> None:
        """Test def statement."""
        assert """\
def link(url, text):
    _b = []; w = _b.append; w('        <a href="'); w(url); \
w('">'); w(text); w('</a>\\n')
    return ''.join(_b)
super_defs['link'] = link; link = local_defs.setdefault('link', link)
w('    Please '); w(link('/en/signin', 'sign in')); w('.\\n')\
""" == self.build_source(
            """\
    @def link(url, text):
        <a href="@url">@text</a>
        @#ignore
    @end
    Please @link('/en/signin', 'sign in').
"""
        )

    def test_def_empty(self) -> None:
        """Test def statement with empty function body."""
        assert """\
def title():return ''
super_defs['title'] = title; title = local_defs.setdefault('title', title)
w(title()); w('.')""" == self.build_source(
            """\
@def title():
\
@end
@title()."""
        )

    def test_def_single_markup(self) -> None:
        """Test def statement with a single return markup."""
        assert """\
def title():
    return '    Hello\\n'
super_defs['title'] = title; title = local_defs.setdefault('title', title)
w(title()); w('.')""" == self.build_source(
            """\
@def title():
    Hello
@end
@title()."""
        )

    def test_def_single_var(self) -> None:
        """Test def statement with a single return var."""
        expected = """\
def title(x):
    _b = []; w = _b.append; w(x); return ''.join(_b)
super_defs['title'] = title; title = local_defs.setdefault('title', title); \
w(title()); w('.')"""
        assert expected == self.build_source(
            """\
@def title(x):
@x\
@end
@title()."""
        )
        assert expected == self.build_source(
            """\
@def title(x):
@{ x }\
@end
@title()."""
        )

    def test_render(self) -> None:
        """Test build_render."""
        assert """\
def render(ctx, local_defs, super_defs):

    return 'Hello'""" == self.build_render(
            "Hello"
        )

    def test_render_empty(self) -> None:
        """Test build_render with return of empty string."""
        assert """\
def render(ctx, local_defs, super_defs):
    return ''""" == self.build_render(
            ""
        )
        """ Test build_render with return of empty string.
        """
        assert """\
def render(ctx, local_defs, super_defs):
    return ''""" == self.build_render(
            ""
        )

    def test_render_var(self) -> None:
        """Test build_render with return of var."""
        assert """\
def render(ctx, local_defs, super_defs):
    _b = []; w = _b.append
    w(h(a))
    return ''.join(_b)""" == self.build_render(
            "@a!h"
        )

    def test_extends(self) -> None:
        """Test build_extends."""
        assert """\
def render(ctx, local_defs, super_defs):
    return _r("base.html", ctx, local_defs, super_defs)\
""" == self.build_render(
            """\
@extends("base.html")
"""
        )

    def test_extends_with_require(self) -> None:
        """Test build_extends with require token."""
        assert """\
def render(ctx, local_defs, super_defs):


    path_for = ctx['path_for']
    return _r("base.html", ctx, local_defs, super_defs)\
""" == self.build_render(
            """\
@extends("base.html")
@require(path_for)
"""
        )

    def test_extends_with_import(self) -> None:
        """Test build_extends with import token."""
        assert """\
def render(ctx, local_defs, super_defs):


    widget = _i('shared/snippet/widget.html')
    return _r("base.html", ctx, local_defs, super_defs)\
""" == self.build_render(
            """\
@extends("base.html")
@import 'shared/snippet/widget.html' as widget
"""
        )

    def test_extends_with_from(self) -> None:
        """Test build_extends with from token."""
        assert """\
def render(ctx, local_defs, super_defs):


    menu_item = _i('shared/snippet/widget.html').local_defs['menu_item']
    return _r("base.html", ctx, local_defs, super_defs)\
""" == self.build_render(
            """\
@extends("base.html")
@from 'shared/snippet/widget.html' import menu_item
"""
        )


class TemplateTestCase(unittest.TestCase):
    """Test the ``CoreExtension`` compiled templates."""

    def render(
        self,
        source: str,
        ctx: typing.Optional[typing.Mapping[str, typing.Any]] = None,
    ) -> str:
        loader = DictLoader({"test.html": source})
        engine = Engine(loader=loader, extensions=[CoreExtension()])
        template = engine.get_template("test.html")
        return template.render(ctx or {})

    def test_markup(self) -> None:
        assert "Hello" == self.render("Hello")
        assert "" == self.render("")
        assert "" == self.render("\\\n")

    def test_line_join(self) -> None:
        assert "a b" == self.render("a \\\nb")

    def test_line_join_escape(self) -> None:
        assert "a \\\nb \\\nc" == self.render("a \\\\\nb \\\\\nc")

    def test_comment(self) -> None:
        assert "Hello World" == self.render(
            """\
Hello\\
@# comment
 World"""
        )

    def test_var(self) -> None:
        ctx = {"username": "John"}
        expected = "Welcome, John!"
        assert expected == self.render(
            """\
@require(username)
Welcome, @username!""",
            ctx,
        )
        assert expected == self.render(
            """\
@require(username)
Welcome, @{ username }!""",
            ctx,
        )

    def test_var_unicode(self) -> None:
        ctx = {"_": lambda x: x}
        assert hello == self.render('@require(_)\n@_("' + hello + '")', ctx)

    def test_rvalue_unicode(self) -> None:
        ctx = {"_": lambda x: x}
        assert hello == self.render('@require(_)\n@{_("' + hello + '")}', ctx)

    def test_var_filter(self) -> None:
        ctx = {"age": 36}
        expected = "age: 36"
        assert expected == self.render(
            """\
@require(age)
age: @age!s""",
            ctx,
        )
        assert expected == self.render(
            """\
@require(age)
age: @{ age !! s }""",
            ctx,
        )

    def test_rvalue(self) -> None:
        ctx = {"accepted": True}
        assert "YES" == self.render(
            """\
@require(accepted)
@{ accepted and 'YES' or 'NO' }""",
            ctx,
        )
        ctx2 = {"age": 36}
        assert "OK" == self.render(
            """\
@require(age)
@{ (age > 20 and age < 120) and 'OK' or '' }""",
            ctx2,
        )

    def test_rvalue_filter(self) -> None:
        ctx = {"n": 36}
        assert "1" == self.render(
            """\
@require(n)
@{ n > 0 and 1 or -1 !! s }""",
            ctx,
        )

    def test_if(self) -> None:
        src = """\
@require(n)
@if n > 0:
    Positive\\
@elif n == 0:
    Zero\\
@else:
    Negative\\
@end
"""
        assert "    Positive" == self.render(src, {"n": 1})
        assert "    Zero" == self.render(src, {"n": 0})
        assert "    Negative" == self.render(src, {"n": -1})

    def test_for(self) -> None:
        ctx = {"colors": ["red", "yellow"]}
        assert "    red\n    yellow\n" == self.render(
            """\
@require(colors)
@for color in colors:
    @color
@end
""",
            ctx,
        )

    def test_def(self) -> None:
        assert "Welcome, John!" == self.render(
            """\
@def welcome(name):
Welcome, @name!\\
@end
@welcome('John')"""
        )

    def test_def_empty(self) -> None:
        assert "." == self.render(
            """\
@def title():
@end
@title()."""
        )

    def test_def_syntax_error_compound(self) -> None:
        self.assertRaises(
            SyntaxError,
            lambda: self.render(
                """\
@def welcome(name):
@if name:
Welcome, @name!\\
@end
@end
@welcome('John')"""
            ),
        )

    def test_def_no_syntax_error(self) -> None:
        assert "Welcome, John!" == self.render(
            """\
@def welcome(name):
@#ignore
@if name:
Welcome, @name!\\
@end
@end
@welcome('John')"""
        )
        assert "\nWelcome, John!" == self.render(
            """\
@def welcome(name):

@if name:
Welcome, @name!\\
@end
@end
@welcome('John')"""
        )


class MultiTemplateTestCase(unittest.TestCase):
    """Test the ``CoreExtension`` compiled templates."""

    def setUp(self) -> None:
        self.templates: typing.Dict[str, str] = {}
        self.engine = Engine(
            loader=DictLoader(templates=self.templates),
            extensions=[CoreExtension()],
        )

    def render(self, name: str, ctx: typing.Mapping[str, typing.Any]) -> str:
        template = self.engine.get_template(name)
        return template.render(ctx)

    def test_extends(self) -> None:
        self.templates.update(
            {
                "master.html": """\
@def say_hi(name):
    Hello, @name!
@end
@say_hi('John')""",
                "tmpl.html": """\
@extends('master.html')
@def say_hi(name):
    Hi, @name!
@end
""",
            }
        )
        assert "    Hi, John!\n" == self.render("tmpl.html", {})
        assert "    Hello, John!\n" == self.render("master.html", {})

    def test_extends_dynamic(self) -> None:
        self.templates.update(
            {
                "master.html": """\
@def say_hi(name):
    Hello, @name!
@end
@say_hi('John')""",
                "tmpl.html": """
@require(master)
@extends(master)
@def say_hi(name):
    Hi, @name!
@end
""",
            }
        )
        assert "    Hi, John!\n" == self.render(
            "tmpl.html", {"master": "master.html"}
        )
        assert "    Hello, John!\n" == self.render("master.html", {})

    def test_super(self) -> None:
        self.templates.update(
            {
                "master.html": """\
@def say_hi(name):
    Hello, @name!\
@end
@say_hi('John')""",
                "tmpl.html": """\
@extends('master.html')
@def say_hi(name):
    @super_defs['say_hi'](name)!!\
@end
""",
            }
        )
        assert "        Hello, John!!!" == self.render("tmpl.html", {})

    def test_include(self) -> None:
        self.templates.update(
            {
                "footer.html": """\
@require(name)
Thanks, @name""",
                "tmpl.html": """\
Welcome to my site.
@include('footer.html')
""",
            }
        )
        ctx = {"name": "John"}
        assert """\
Welcome to my site.
Thanks, John""" == self.render(
            "tmpl.html", ctx
        )
        assert "Thanks, John" == self.render("footer.html", ctx)

    def test_import(self) -> None:
        self.templates.update(
            {
                "helpers.html": """\
@def say_hi(name):
Hi, @name\
@end""",
                "tmpl.html": """\
@import 'helpers.html' as helpers
@helpers.say_hi('John')""",
            }
        )
        assert """\
Hi, John""" == self.render(
            "tmpl.html", {}
        )

    def test_import_dynamic(self) -> None:
        self.templates.update(
            {
                "helpers.html": """\
@def say_hi(name):
Hi, @name\
@end""",
                "tmpl.html": """\
@require(helpers_impl)
@import helpers_impl as helpers
@helpers.say_hi('John')""",
            }
        )
        assert """\
Hi, John""" == self.render(
            "tmpl.html", {"helpers_impl": "helpers.html"}
        )

    def test_from_import(self) -> None:
        self.templates.update(
            {
                "helpers.html": """\
@def say_hi(name):
Hi, @name\
@end""",
                "tmpl.html": """\
@from 'helpers.html' import say_hi
@say_hi('John')""",
            }
        )
        assert """\
Hi, John""" == self.render(
            "tmpl.html", {}
        )

    def test_from_import_dynamic(self) -> None:
        self.templates.update(
            {
                "helpers.html": """\
@def say_hi(name):
Hi, @name\
@end""",
                "tmpl.html": """\
@require(helpers_impl)
@from helpers_impl import say_hi
@say_hi('John')""",
            }
        )
        assert """\
Hi, John""" == self.render(
            "tmpl.html", {"helpers_impl": "helpers.html"}
        )

    def test_from_import_as(self) -> None:
        self.templates.update(
            {
                "share/helpers.html": """\
@def say_hi(name):
Hi, @name\
@end""",
                "tmpl.html": """\
@from 'share/helpers.html' import say_hi as hi
@hi('John')""",
            }
        )
        assert """\
Hi, John""" == self.render(
            "tmpl.html", {}
        )
