"""Test AnnotatedInstance model for serialization and deserialization."""

import json
from pathlib import Path

import pytest
from shapely.geometry import Polygon
from tspn_bnb2.schemas import AnnotatedInstance, PolygonAnnotation


def test_create_with_polygons():
    """Test creating an AnnotatedInstance with Shapely Polygons."""
    poly1 = Polygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = Polygon([(2, 2), (3, 2), (3, 3), (2, 3)])

    instance = AnnotatedInstance(
        polygons=[poly1, poly2],
        meta={"source": "test"},
        annotations={
            0: PolygonAnnotation(hull_index=0, intersections=[1]),
            1: PolygonAnnotation(hull_index=1, intersections=[0]),
        },
    )

    assert len(instance.polygons) == 2
    assert isinstance(instance.polygons[0], Polygon)
    assert isinstance(instance.polygons[1], Polygon)
    assert instance.polygons[0].area == pytest.approx(1.0)
    assert instance.polygons[1].area == pytest.approx(1.0)


def test_create_with_wkt_strings():
    """Test creating an AnnotatedInstance with WKT strings."""
    wkt1 = "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))"
    wkt2 = "POLYGON ((2 2, 3 2, 3 3, 2 3, 2 2))"

    instance = AnnotatedInstance(polygons=[wkt1, wkt2], meta={"source": "test"})

    assert len(instance.polygons) == 2
    assert isinstance(instance.polygons[0], Polygon)
    assert isinstance(instance.polygons[1], Polygon)
    assert instance.polygons[0].area == pytest.approx(1.0)
    assert instance.polygons[1].area == pytest.approx(1.0)


def test_serialization_to_dict():
    """Test serialization to dictionary with WKT strings."""
    poly1 = Polygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = Polygon([(2, 2), (3, 2), (3, 3), (2, 3)])

    instance = AnnotatedInstance(
        polygons=[poly1, poly2],
        meta={"source": "test", "count": 2},
        annotations={
            0: PolygonAnnotation(hull_index=0, intersections=[1]),
            1: PolygonAnnotation(hull_index=-1, intersections=[]),
        },
    )

    data = instance.model_dump()

    assert "polygons" in data
    assert len(data["polygons"]) == 2
    assert all(isinstance(p, str) for p in data["polygons"])
    assert "POLYGON" in data["polygons"][0]
    assert data["meta"] == {"source": "test", "count": 2}
    assert 0 in data["annotations"]
    assert data["annotations"][0]["hull_index"] == 0
    assert data["annotations"][0]["intersections"] == [1]


def test_serialization_to_json():
    """Test serialization to JSON string."""
    poly1 = Polygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = Polygon([(2, 2), (3, 2), (3, 3), (2, 3)])

    instance = AnnotatedInstance(
        polygons=[poly1, poly2],
        meta={"source": "test"},
        annotations={0: PolygonAnnotation(hull_index=0, intersections=[1])},
    )

    json_str = instance.model_dump_json()
    data = json.loads(json_str)

    assert "polygons" in data
    assert len(data["polygons"]) == 2
    assert all(isinstance(p, str) for p in data["polygons"])
    assert "POLYGON" in data["polygons"][0]


def test_deserialization_from_dict():
    """Test deserialization from dictionary with WKT strings."""
    data = {
        "polygons": ["POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", "POLYGON ((2 2, 3 2, 3 3, 2 3, 2 2))"],
        "meta": {"source": "test"},
        "annotations": {
            "0": {"hull_index": 0, "intersections": [1]},
            "1": {"hull_index": 1, "intersections": [0]},
        },
    }

    instance = AnnotatedInstance.model_validate(data)

    assert len(instance.polygons) == 2
    assert all(isinstance(p, Polygon) for p in instance.polygons)
    assert instance.polygons[0].area == pytest.approx(1.0)
    assert instance.get_annotation(0).hull_index == 0
    assert instance.get_annotation(0).intersections == [1]


def test_deserialization_from_json():
    """Test deserialization from JSON string."""
    json_str = """{
        "polygons": [
            "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
            "POLYGON ((2 2, 3 2, 3 3, 2 3, 2 2))"
        ],
        "meta": {"source": "test", "count": 2},
        "annotations": {
            "0": {"hull_index": 0, "intersections": [1]}
        }
    }"""

    instance = AnnotatedInstance.model_validate_json(json_str)

    assert len(instance.polygons) == 2
    assert all(isinstance(p, Polygon) for p in instance.polygons)
    assert instance.meta["count"] == 2
    assert instance.get_annotation(0).hull_index == 0


def test_roundtrip_serialization():
    """Test that serialization and deserialization preserve data."""
    poly1 = Polygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = Polygon([(2, 2), (3, 2), (3, 3), (2, 3)])

    original = AnnotatedInstance(
        polygons=[poly1, poly2],
        meta={"source": "test", "version": 1},
        annotations={
            0: PolygonAnnotation(hull_index=0, intersections=[1]),
            1: PolygonAnnotation(hull_index=1, intersections=[0]),
        },
    )

    # Serialize to JSON and back
    json_str = original.model_dump_json()
    restored = AnnotatedInstance.model_validate_json(json_str)

    assert len(restored.polygons) == len(original.polygons)
    assert restored.polygons[0].equals(original.polygons[0])
    assert restored.polygons[1].equals(original.polygons[1])
    assert restored.meta == original.meta
    assert restored.get_annotation(0).hull_index == original.get_annotation(0).hull_index
    assert restored.get_annotation(0).intersections == original.get_annotation(0).intersections


def test_meta_with_various_types():
    """Test that meta field accepts various value types."""
    poly = Polygon([(0, 0), (1, 0), (1, 1), (0, 1)])

    instance = AnnotatedInstance(
        polygons=[poly],
        meta={
            "string_value": "test",
            "int_value": 42,
            "float_value": 3.14,
            "bool_value": True,
            "list_value": [1, 2, 3],
            "dict_value": {"nested": "value"},
            "bbox": {"min_x": 0.0, "min_y": 0.0, "max_x": 1.0, "max_y": 1.0},
        },
    )

    assert instance.meta["string_value"] == "test"
    assert instance.meta["int_value"] == 42
    assert instance.meta["float_value"] == 3.14
    assert instance.meta["bool_value"] is True
    assert instance.meta["list_value"] == [1, 2, 3]
    assert instance.meta["dict_value"] == {"nested": "value"}
    assert instance.meta["bbox"]["min_x"] == 0.0

    # Test roundtrip
    json_str = instance.model_dump_json()
    restored = AnnotatedInstance.model_validate_json(json_str)
    assert restored.meta == instance.meta


def test_load_att48():
    """Test loading the att48 instance from JSON file."""
    att48_path = Path(__file__).parent / "instances" / "att48.json"

    if not att48_path.exists():
        pytest.skip("att48.json not found")

    with open(att48_path) as f:
        data = json.load(f)

    # Adapt the data format to AnnotatedInstance
    instance_data = {"polygons": data["sites"], "meta": data["meta"], "annotations": {}}

    instance = AnnotatedInstance.model_validate(instance_data)

    assert len(instance.polygons) == 48
    assert all(isinstance(p, Polygon) for p in instance.polygons)
    assert instance.meta["source"] == "public_instance_set"
    assert instance.meta["n_polygons"] == 48
    assert instance.meta["bbox"]["min_x"] == 10.0
    assert instance.meta["bbox"]["max_x"] == 7762.0
    assert isinstance(instance.meta["original_points"], list)


def test_annotation_helpers():
    """Test annotation helper methods."""
    poly1 = Polygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = Polygon([(2, 2), (3, 2), (3, 3), (2, 3)])
    poly3 = Polygon([(4, 4), (5, 4), (5, 5), (4, 5)])

    instance = AnnotatedInstance(
        polygons=[poly1, poly2, poly3],
        annotations={
            0: PolygonAnnotation(hull_index=0, intersections=[2]),
            1: PolygonAnnotation(hull_index=-1, intersections=[]),
            2: PolygonAnnotation(hull_index=1, intersections=[0]),
        },
    )

    # Test get_annotation
    assert instance.get_annotation(0).hull_index == 0
    assert instance.get_annotation(1).hull_index == -1
    assert instance.get_annotation(999) is None

    # Test set_annotation
    instance.set_annotation(1, PolygonAnnotation(hull_index=2, intersections=[0, 2]))
    assert instance.get_annotation(1).hull_index == 2
    assert instance.get_annotation(1).intersections == [0, 2]

    # Test num_polygons
    assert instance.num_polygons() == 3

    # Test hull_polygons
    hull_indices = instance.hull_polygons()
    assert hull_indices == [0, 2, 1]  # Sorted by hull_index


def test_empty_annotations():
    """Test instance with no annotations."""
    poly = Polygon([(0, 0), (1, 0), (1, 1), (0, 1)])

    instance = AnnotatedInstance(polygons=[poly], meta={"source": "test"})

    assert len(instance.annotations) == 0
    assert instance.get_annotation(0) is None
    assert instance.hull_polygons() == []


def test_polygon_annotation_defaults():
    """Test PolygonAnnotation default values."""
    ann = PolygonAnnotation(hull_index=0)

    assert ann.hull_index == 0
    assert ann.intersections == []


def test_invalid_polygon_type():
    """Test that invalid polygon types raise an error."""
    with pytest.raises(TypeError, match="Expected str or Polygon"):
        AnnotatedInstance(
            polygons=[123],  # Invalid type
            meta={},
        )
