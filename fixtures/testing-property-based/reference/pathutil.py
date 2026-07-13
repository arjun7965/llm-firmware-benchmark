"""Trusted implementation used to calibrate property-test answers."""


def normalize_path(path: str) -> str:
    """Normalize a virtual POSIX path according to the benchmark contract."""
    if "\0" in path:
        raise ValueError("paths cannot contain NUL")

    components = []
    for component in path.split("/"):
        if component == "" or component == ".":
            continue
        if component == "..":
            if components:
                components.pop()
            continue
        components.append(component)
    return "/" + "/".join(components)
