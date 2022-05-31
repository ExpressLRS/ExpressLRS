import typing

from external.wheezy.template.comp import allocate_lock  # type: ignore[attr-defined]
from external.wheezy.template.engine import Engine
from external.wheezy.template.loader import ChainLoader, DictLoader
from external.wheezy.template.typing import Loader, SupportsRender


class Preprocessor(object):
    """Preprocess templates with ``engine`` and vary runtime templates
    by ``key_factory`` function using ``runtime_engine_factory``.
    """

    def __init__(
        self,
        runtime_engine_factory: typing.Callable[[Loader], Engine],
        engine: Engine,
        key_factory: typing.Callable[[typing.Mapping[str, typing.Any]], str],
    ) -> None:
        self.lock = allocate_lock()
        self.runtime_engines: typing.Dict[str, Engine] = {}
        self.runtime_engine_factory = runtime_engine_factory
        self.engine = engine
        self.loader = engine.loader
        self.key_factory = key_factory
        template_class = self.engine.template_class
        self.engine.template_class = lambda name, _: template_class(
            name,
            lambda ctx, local_defs, super_defs: self.render(
                name, ctx, local_defs, super_defs
            ),
        )

    def get_template(self, name: str) -> SupportsRender:
        return self.engine.get_template(name)

    def render(
        self,
        name: str,
        ctx: typing.Mapping[str, typing.Any],
        local_defs: typing.Mapping[str, typing.Any],
        super_defs: typing.Mapping[str, typing.Any],
    ) -> str:
        try:
            runtime_engine = self.runtime_engines[self.key_factory(ctx)]
        except KeyError:
            runtime_engine = self.ensure_runtime_engine(self.key_factory(ctx))
        try:
            return runtime_engine.renders[name](ctx, local_defs, super_defs)
        except KeyError:
            self.preprocess_template(runtime_engine, name, ctx)
            return runtime_engine.renders[name](ctx, local_defs, super_defs)

    def remove(self, name: str) -> None:
        self.lock.acquire(True)
        try:
            self.engine.remove(name)
            for runtime_engine in self.runtime_engines.values():
                runtime_engine.remove(name)
        finally:
            self.lock.release()

    # region: internal details

    def ensure_runtime_engine(self, key: str) -> Engine:
        self.lock.acquire(True)
        try:
            engines = self.runtime_engines
            if key in engines:  # pragma: nocover
                return engines[key]
            engine = engines[key] = self.runtime_engine_factory(
                ChainLoader([DictLoader({}), self.engine.loader])
            )

            def render(
                name: str,
                ctx: typing.Mapping[str, typing.Any],
                local_defs: typing.Mapping[str, typing.Any],
                super_defs: typing.Mapping[str, typing.Any],
            ) -> str:
                try:
                    return engine.renders[name](ctx, local_defs, super_defs)
                except KeyError:
                    self.preprocess_template(engine, name, ctx)
                    return engine.renders[name](ctx, local_defs, super_defs)

            engine.global_vars["_r"] = render
            return engine
        finally:
            self.lock.release()

    def preprocess_template(
        self,
        runtime_engine: Engine,
        name: str,
        ctx: typing.Mapping[str, typing.Any],
    ) -> None:
        self.lock.acquire(True)
        try:
            if name not in runtime_engine.renders:
                source = self.engine.render(name, ctx, {}, {})
                loader = runtime_engine.loader.loaders[0]  # type: ignore
                loader.templates[name] = source
                runtime_engine.compile_template(name)
                del loader.templates[name]
        finally:
            self.lock.release()
