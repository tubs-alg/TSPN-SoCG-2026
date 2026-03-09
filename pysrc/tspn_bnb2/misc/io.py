"""Collection of IO Helper functions."""

import functools
import json
from collections.abc import Generator
from typing import Any, BinaryIO
from zipfile import ZIP_LZMA, ZipFile

from tspn_bnb2.core._tspn_bindings import Instance, Trajectory, parse_site_wkt


def convert_number(val: Any) -> Any:
    """Try to convert potential string number into a number type."""
    if not isinstance(val, str):
        return val
    try:
        return int(val)
    except ValueError:
        try:
            return float(val)
        except ValueError:
            return val


def write_instance(
    file: BinaryIO,
    instance: Instance,
    instancename: str,
    meta: dict | None = None,
) -> None:
    """Store an instance in a JSON-file."""
    data = {
        "type": "tspn_instance",
        "instancename": instancename,
        "meta": meta or {},
        "sites": [f"{site.__repr__()}" for site in instance.sites()],
        "path": instance.is_path(),
    }
    file.write(json.dumps(data, indent=2).encode())


def read_instance(file: BinaryIO) -> tuple[str, dict, Instance]:
    """Read a JSON-File and construct an Instance from it."""
    data = json.load(file)
    if data.get("type", "tspn_instance") == "tspn_instance":
        sites = data.get("sites", None)
        if not sites:
            sites = data.get("polygons", None)
        if not sites:
            sites = data.get("polys", None)

        return (
            data.get("instancename", ""),
            data.get("meta", {}),
            Instance([parse_site_wkt(wkt) for wkt in sites], data.get("path", False)),
        )
    raise ValueError("attempted file not a tspn instance")


def parse_tour(wktstr: str) -> Trajectory:
    """Parse wkt into trajectory."""
    return Trajectory.from_wkt(wktstr)


# InstanceDb from https://github.com/d-krupke/AlgBench/blob/main/examples/graph_coloring/_utils/__init__.py
class InstanceDb:
    """Simple helper to store and load the instances.

    Compressed zip to save disk space and making it small enough for git.
    """

    def __init__(self, path: str) -> None:
        """Initialize a new instance DB by setting its DB file."""
        self.path: str = path

    @functools.lru_cache(10)
    def __getitem__(self, name: str) -> Instance:
        """Get instance from DB by instance name."""
        with ZipFile(self.path, "r") as z, z.open(name + ".json", "r") as f:
            return read_instance(f)

    def __setitem__(self, name: str, instance_meta: tuple[Instance, dict | None]) -> None:
        """Add instance + meta to instance DB."""
        self.__getitem__.cache_clear()
        with ZipFile(self.path, compression=ZIP_LZMA, mode="a") as instance_archive:  # noqa: SIM117
            with instance_archive.open(name + ".json", "w") as f:
                write_instance(f, instance_meta[0], name, instance_meta[1])

    def __iter__(self) -> Generator[str]:
        """Iterate all instance names (=keys) in DB."""
        with ZipFile(self.path, "r") as z:
            for f in z.filelist:
                yield f.filename[:-5]
