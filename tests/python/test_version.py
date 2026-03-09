"""Verify that __version__ and setup.py stay in sync."""

import ast
from pathlib import Path


def _get_setup_version() -> str:
    """Parse the version string from setup.py without importing it."""
    setup_py = Path(__file__).parent.parent.parent / "setup.py"
    tree = ast.parse(setup_py.read_text())
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and getattr(node.func, "id", None) == "setup":
            for kw in node.keywords:
                if kw.arg == "version":
                    return ast.literal_eval(kw.value)
    raise RuntimeError("Could not find version= in setup.py")


def test_version_in_sync():
    """Ensure tspn_bnb2.__version__ matches setup.py version."""
    import tspn_bnb2

    assert tspn_bnb2.__version__ == _get_setup_version(), (
        f"Version mismatch: tspn_bnb2.__version__={tspn_bnb2.__version__!r} "
        f"but setup.py has {_get_setup_version()!r}"
    )
