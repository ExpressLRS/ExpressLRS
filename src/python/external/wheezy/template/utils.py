def find_all_balanced(text: str, start: int = 0) -> int:
    """Finds balanced ``([`` with ``])`` assuming
    that ``start`` is pointing to ``(`` or ``[`` in ``text``.
    """
    if start >= len(text) or text[start] not in "([":
        return start
    while 1:
        pos = find_balanced(text, start)
        pos = find_balanced(text, pos, "[", "]")
        if pos != start:
            start = pos
        else:
            return pos


def find_balanced(
    text: str, start: int = 0, start_sep: str = "(", end_sep: str = ")"
) -> int:
    """Finds balanced ``start_sep`` with ``end_sep`` assuming
    that ``start`` is pointing to ``start_sep`` in ``text``.
    """
    if start >= len(text) or start_sep != text[start]:
        return start
    balanced = 1
    pos = start + 1
    while pos < len(text):
        token = text[pos]
        pos += 1
        if token == end_sep:
            if balanced == 1:
                return pos
            balanced -= 1
        elif token == start_sep:
            balanced += 1
    return start


def print_source(source: str, lineno: int = 1) -> None:  # pragma: nocover
    lines = []
    for line in source.split("\n"):
        lines.append("%02d " % lineno + line)
        lineno += line.count("\n") + 1
    print("\n".join(lines))
