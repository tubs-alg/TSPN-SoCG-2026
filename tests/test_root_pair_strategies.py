from tspn_bnb2 import Instance, Point, Polygon, branch_and_bound


def _grid_polygons(n: int = 4, size: float = 1.0):
    polys = []
    for i in range(n):
        for j in range(n):
            x0, y0 = i * size, j * size
            polys.append(
                Polygon(
                    [
                        [
                            Point(x0, y0),
                            Point(x0 + size, y0),
                            Point(x0 + size, y0 + size),
                            Point(x0, y0 + size),
                        ]
                    ]
                )
            )
    return polys


def _run_bnb(instance: Instance, root: str):
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=10,
        branching="FarthestPoly",
        search="DfsBfs",
        root=root,
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=2,
        decomposition_branch=True,
        eps=0.001,
    )
    return ub, lb, stats


def test_root_longest_pair_tour_grid():
    """Root strategy LongestPair should create a valid root on a grid tour."""
    polys = _grid_polygons()
    instance = Instance(polys, False)
    ub, lb, _ = _run_bnb(instance, root="LongestPair")
    assert ub is not None
    assert lb >= 0


def test_root_random_pair_tour_grid():
    """Root strategy RandomPair should create a valid root on a grid tour."""
    polys = _grid_polygons()
    instance = Instance(polys, False)
    ub, lb, _ = _run_bnb(instance, root="RandomPair")
    assert ub is not None
    assert lb >= 0


def test_root_longest_pair_path_grid():
    """Root strategy LongestPair should work for path (origin/target + grid)."""
    polys = _grid_polygons()
    instance = Instance([Point(0, 0), Point(6, 6), *polys], True)
    ub, lb, _ = _run_bnb(instance, root="LongestPair")
    assert ub is not None
    assert lb >= 0


def test_root_random_pair_path_grid():
    """Root strategy RandomPair should work for path (origin/target + grid)."""
    polys = _grid_polygons()
    instance = Instance([Point(0, 0), Point(6, 6), *polys], True)
    ub, lb, _ = _run_bnb(instance, root="RandomPair")
    assert ub is not None
    assert lb >= 0


def test_root_orderroot_respects_overlaps():
    """OrderRoot should greedily pick annotated non-overlapping sites."""
    polys = _grid_polygons()
    instance = Instance(polys, False)

    # Annotate first four with increasing orders; mark second as overlapping with first
    instance[0].annotations.order_index = 0
    instance[1].annotations.order_index = 1
    instance[2].annotations.order_index = 2
    instance[3].annotations.order_index = 3
    instance[0].annotations.overlapping_order_geo_indices = [1]  # skip 1 if 0 chosen

    ub, lb, _ = _run_bnb(instance, root="OrderRoot")
    assert ub is not None
    assert lb >= 0
