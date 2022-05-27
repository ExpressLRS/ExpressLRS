import os
import os.path
import stat
import time
import typing

from external.wheezy.template.engine import Engine
from external.wheezy.template.typing import Loader, SupportsRender


class FileLoader(Loader):
    """Loads templates from file system.

    ``directories`` - search path of directories to scan for template.
    ``encoding`` - decode template content per encoding.
    """

    def __init__(
        self, directories: typing.List[str], encoding: str = "UTF-8"
    ) -> None:
        searchpath: typing.List[str] = []
        for path in directories:
            abspath = os.path.abspath(path)
            assert os.path.exists(abspath)
            assert os.path.isdir(abspath)
            searchpath.append(abspath)
        self.searchpath = searchpath
        self.encoding = encoding

    def list_names(self) -> typing.Tuple[str, ...]:
        """Return a list of names relative to directories. Ignores any files
        and directories that start with dot.
        """
        names = []
        for path in self.searchpath:
            pathlen = len(path) + 1
            for dirpath, dirnames, filenames in os.walk(path):
                for i in [
                    i
                    for i, name in enumerate(dirnames)
                    if name.startswith(".")
                ]:
                    del dirnames[i]
                for filename in filenames:
                    if filename.startswith("."):
                        continue
                    name = os.path.join(dirpath, filename)[pathlen:]
                    name = name.replace("\\", "/")
                    names.append(name)
        return tuple(sorted(names))

    def get_fullname(self, name: str) -> typing.Optional[str]:
        """Returns a full path by a template name."""
        for path in self.searchpath:
            filename = os.path.join(path, name)
            if not os.path.exists(filename):
                continue
            if not os.path.isfile(filename):
                continue
            return filename
        else:
            return None

    def load(self, name: str) -> typing.Optional[str]:
        """Loads a template by name from file system."""
        filename = self.get_fullname(name)
        if filename:
            f = open(filename, "rb")
            try:
                return f.read().decode(self.encoding)
            finally:
                f.close()
        return None


class DictLoader(Loader):
    """Loads templates from python dictionary.

    ``templates`` - a dict where key corresponds to template name and
    value to template content.
    """

    def __init__(self, templates: typing.Mapping[str, str]) -> None:
        self.templates = templates

    def list_names(self) -> typing.Tuple[str, ...]:
        """List all keys from internal dict."""
        return tuple(sorted(self.templates.keys()))

    def load(self, name: str) -> typing.Optional[str]:
        """Returns template by name."""
        if name not in self.templates:
            return None
        return self.templates[name]


class ChainLoader(Loader):
    """Loads templates from ``loaders`` until first succeed."""

    def __init__(self, loaders: typing.List[Loader]) -> None:
        self.loaders = loaders

    def list_names(self) -> typing.Tuple[str, ...]:
        """Returns as list of names from all loaders."""
        names = set()
        for loader in self.loaders:
            names |= set(loader.list_names())
        return tuple(sorted(names))

    def load(self, name: str) -> typing.Optional[str]:
        """Returns template by name from the first loader that succeed."""
        for loader in self.loaders:
            source = loader.load(name)
            if source is not None:
                return source
        return None


class PreprocessLoader(Loader):
    """Performs preprocessing of loaded template."""

    def __init__(
        self,
        engine: Engine,
        ctx: typing.Optional[typing.Mapping[str, typing.Any]] = None,
    ) -> None:
        self.engine = engine
        self.ctx = ctx or {}

    def list_names(self) -> typing.Tuple[str, ...]:
        return self.engine.loader.list_names()

    def load(self, name: str) -> str:
        return self.engine.render(name, self.ctx, {}, {})


def autoreload(engine: Engine, enabled: bool = True) -> Engine:
    """Auto reload template if changes are detected in file.

    Limitation: master (inherited), imported and preprocessed templates.

    It is recommended to use application server that supports
    file reload instead.
    """
    if not enabled:
        return engine
    return AutoReloadProxy(engine)


# region: internal details


class AutoReloadProxy(Engine):
    def __init__(self, engine: Engine):
        from warnings import warn

        self.engine = engine
        self.names: typing.Dict[str, int] = {}
        warn(
            "autoreload limitation: master (inherited), imported "
            "and preprocessed templates. It is recommended to use "
            "application server that supports file reload instead.",
            stacklevel=3,
        )

    def get_template(self, name: str) -> SupportsRender:
        if self.file_changed(name):
            self.remove(name)
        return self.engine.get_template(name)

    def render(
        self,
        name: str,
        ctx: typing.Mapping[str, typing.Any],
        local_defs: typing.Mapping[str, typing.Any],
        super_defs: typing.Mapping[str, typing.Any],
    ) -> str:
        if self.file_changed(name):
            self.remove(name)
        return self.engine.render(name, ctx, local_defs, super_defs)

    def remove(self, name: str) -> None:
        self.engine.remove(name)

    # region: internal details

    def __getattr__(self, name: str) -> typing.Any:
        return getattr(self.engine, name)

    def file_changed(self, name: str) -> bool:
        try:
            last_known_stamp = self.names[name]
            current_time = int(time.time())
            if current_time - last_known_stamp <= 2:
                return False
        except KeyError:
            last_known_stamp = 0

        loader = self.engine.loader
        abspath = loader.get_fullname(name)  # type: ignore[attr-defined]
        if not abspath:
            return False

        last_modified_stamp = os.stat(abspath)[stat.ST_MTIME]
        if last_modified_stamp <= last_known_stamp:
            return False
        self.names[name] = last_modified_stamp
        return True
