"""Trusted answer used to calibrate the property-based testing fixture."""

import pytest
from hypothesis import given, strategies as st

from pathutil import normalize_path


ordinary_component = st.text(
    alphabet=st.characters(blacklist_characters="/\0"),
    min_size=1,
).filter(lambda value: value not in {".", ".."})
path_component = st.one_of(
    ordinary_component,
    st.sampled_from(["", ".", ".."]),
)


@st.composite
def virtual_paths(draw):
    components = draw(st.lists(path_component, max_size=20))
    separators = draw(
        st.lists(
            st.integers(min_value=1, max_value=4),
            min_size=len(components) + 1,
            max_size=len(components) + 1,
        )
    )
    absolute = draw(st.booleans())
    prefix = "/" * separators[0] if absolute else ""
    body = "".join(
        component + "/" * separators[index + 1]
        for index, component in enumerate(components)
    )
    return prefix + body


def oracle(path: str) -> str:
    """Independent stack-machine oracle for virtual POSIX paths."""
    if any(character == "\0" for character in path):
        raise ValueError("NUL is forbidden")

    resolved = []
    component = []
    for character in path + "/":
        if character != "/":
            component.append(character)
            continue
        token = "".join(component)
        component.clear()
        if token in {"", "."}:
            continue
        if token == "..":
            if resolved:
                del resolved[-1]
        else:
            resolved.append(token)
    return "/" + "/".join(resolved)


@pytest.mark.parametrize(
    ("path", "expected"),
    [
        ("", "/"),
        (".", "/"),
        ("../../..", "/"),
        ("//a///b/./c", "/a/b/c"),
        ("/a/b/../../../c", "/c"),
        ("cafe\N{COMBINING ACUTE ACCENT}/\N{SNOWMAN}",
         "/cafe\N{COMBINING ACUTE ACCENT}/\N{SNOWMAN}"),
    ],
)
def test_targeted_boundaries(path, expected):
    assert normalize_path(path) == expected


@given(virtual_paths())
def test_matches_independent_oracle(path):
    assert normalize_path(path) == oracle(path)


@given(virtual_paths())
def test_output_is_absolute_confined_and_idempotent(path):
    normalized = normalize_path(path)
    assert normalized.startswith("/")
    assert "//" not in normalized
    assert normalized == "/" or all(
        part not in {"", ".", ".."}
        for part in normalized.split("/")[1:]
    )
    assert normalize_path(normalized) == normalized


@given(st.lists(ordinary_component, max_size=12))
def test_preserves_ordinary_unicode_components(components):
    path = "/".join(components)
    assert normalize_path(path) == "/" + path


@given(virtual_paths())
def test_redundant_root_slash_and_dot_are_metamorphic(path):
    expected = normalize_path(path)
    assert normalize_path("///./" + path) == expected
    assert normalize_path(path + "//./") == expected


@given(virtual_paths(), st.text())
def test_nul_is_always_rejected(path, suffix):
    with pytest.raises(ValueError):
        normalize_path(path + "\0" + suffix)
