from tspn_bnb2.core import Polygon


def test_polygon_wkt_simple():
    wkt_str = "POLYGON((0 0,0 4,4 4,4 0,0 0))"

    polygon = Polygon.from_wkt(wkt_str)

    assert str(polygon) == wkt_str, "WKT serialization mismatch"


def test_polygon_wkt_precision():
    wkt_str = (
        "POLYGON(("
        "787357.41724149184 1443027.0882888259,"
        "787366.39143375191 1443027.2926948301,"
        "787366.69280471222 1443014.0222355993,"
        "787357.72946099809 1443013.8179436575,"
        "787357.41724149184 1443027.0882888259))"
    )

    polygon = Polygon.from_wkt(wkt_str)

    print(polygon)
    assert str(polygon) == wkt_str, "WKT serialization mismatch"
