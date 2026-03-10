"""Pydantic models for annotated instances and solutions with polygon metadata."""

from typing import Any

import numpy as np
from pydantic import BaseModel, ConfigDict, Field, field_serializer, field_validator
from shapely import orient_polygons
from shapely.geometry import LineString, Polygon
from shapely.wkt import loads as load_wkt


class PolygonAnnotation(BaseModel):
    """Annotation for a single polygon.

    Attributes:
        hull_index: Index of this polygon in the convex hull (-1 if not on hull)
        intersections: List of indices of polygons that intersect with this polygon

    """

    hull_index: int = Field(..., description="Index in convex hull, or -1 if not on hull")
    intersections: list[int] = Field(
        default_factory=list, description="Indices of intersecting polygons"
    )


class AnnotatedInstance(BaseModel):
    """Instance with polygon annotations.

    Attributes:
        polygons: List of Shapely Polygon objects (serialized to WKT)
        annotations: Dictionary mapping polygon index to its annotation

    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    polygons: list[Polygon] = Field(..., description="List of Shapely Polygon objects")
    meta: dict[str, Any] = Field(default_factory=dict, description="Metadata for the instance")
    annotations: dict[int, PolygonAnnotation] = Field(
        default_factory=dict, description="Annotations indexed by polygon index"
    )

    @field_validator("polygons", mode="before")
    @classmethod
    def validate_polygons(cls, v: Any) -> list[Polygon]:
        """Convert to shapely polygons, enforce CCW orientation and closed rings."""
        if isinstance(v, list):
            result = []
            for item in v:
                # Convert to polygon
                if isinstance(item, str):
                    poly = load_wkt(item)
                elif isinstance(item, Polygon):
                    poly = item
                else:
                    raise TypeError(f"Expected str or Polygon, got {type(item)}")

                # Ensure closure of the exterior ring
                exterior = list(poly.exterior.coords)
                if exterior[0] != exterior[-1]:
                    exterior.append(exterior[0])

                # Ensure closure of interior rings (holes)
                interiors = []
                for ring in poly.interiors:
                    coords = list(ring.coords)
                    if coords[0] != coords[-1]:
                        coords.append(coords[0])
                    interiors.append(coords)

                poly = Polygon(exterior, interiors)

                poly = orient_polygons(poly, exterior_cw=True)
                result.append(poly)

            return result

        return v

    @field_serializer("polygons")
    def serialize_polygons(self, polygons: list[Polygon]) -> list[str]:
        """Serialize Shapely Polygons to WKT strings."""
        return [poly.wkt for poly in polygons]

    def get_annotation(self, index: int) -> PolygonAnnotation | None:
        """Get annotation for polygon at given index.

        Args:
            index: Index of the polygon

        Returns:
            PolygonAnnotation if exists, None otherwise

        """
        return self.annotations.get(index)

    def set_annotation(self, index: int, annotation: PolygonAnnotation) -> None:
        """Set annotation for polygon at given index.

        Args:
            index: Index of the polygon
            annotation: Annotation to set

        """
        self.annotations[index] = annotation

    def num_polygons(self) -> int:
        """Get the number of polygons in this instance."""
        return len(self.polygons)

    def hull_polygons(self) -> list[int]:
        """Get indices of polygons on the hull.

        Returns:
            List of polygon indices that are on the hull, sorted by hull_index

        """
        hull_polys = [
            (idx, ann.hull_index) for idx, ann in self.annotations.items() if ann.hull_index >= 0
        ]
        return [idx for idx, _ in sorted(hull_polys, key=lambda x: x[1])]

    def derive_instance_type(self) -> str:
        """Derive the instance type from metadata."""
        if "geo_information" in self.meta:
            return "OSM"
        if self.meta["source"] == "public_instance_set":
            return "tessellation"
        if self.meta["source"] == "random":
            return "random"

        return "unknown"


class AnnotatedSolution(BaseModel):
    """Solution with trajectory and metadata.

    Attributes:
        trajectory: Trajectory as a Shapely LineString (serialized to WKT)
        lower_bound: Proven lower bound from the optimization
        upper_bound: Upper bound (length of the trajectory)
        is_optimal: Whether the solution is proven optimal (gap <= eps)
        gap: Optimality gap (upper_bound - lower_bound)
        statistics: Algorithm statistics from branch-and-bound
        is_tour: Whether the trajectory forms a closed tour
        meta: Additional metadata

    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    trajectory: LineString = Field(..., description="Solution trajectory as LineString")
    lower_bound: float = Field(..., description="Proven lower bound")
    upper_bound: float = Field(..., description="Upper bound (trajectory length)")
    is_optimal: bool = Field(..., description="Whether solution is proven optimal")
    gap: float = Field(..., description="Optimality gap")
    statistics: dict[str, Any] = Field(default_factory=dict, description="Algorithm statistics")
    is_tour: bool = Field(..., description="Whether trajectory is a tour")
    meta: dict[str, Any] = Field(default_factory=dict, description="Additional metadata")

    @field_validator("trajectory", mode="before")
    @classmethod
    def validate_trajectory(cls, v: Any) -> LineString:
        """Convert WKT string to Shapely LineString if needed."""
        if isinstance(v, str):
            return load_wkt(v)
        if isinstance(v, LineString):
            return v
        msg = f"Expected str or LineString, got {type(v)}"
        raise ValueError(msg)

    @field_validator("upper_bound", "gap", mode="before")
    @classmethod
    def validate_float_or_inf(cls, v: Any) -> float:
        """Handle None (from JSON null) as infinity during deserialization."""
        if v is None:
            return float("inf")
        return float(v)

    @field_serializer("trajectory")
    def serialize_trajectory(self, trajectory: LineString) -> str:
        """Serialize Shapely LineString to WKT string."""
        return trajectory.wkt

    @property
    def length(self) -> float:
        """Get the length of the trajectory."""
        return self.upper_bound

    @property
    def relative_gap(self) -> float:
        """Get the relative optimality gap (gap / upper_bound)."""
        if self.upper_bound > 0:
            return self.gap / self.upper_bound
        return 0.0

    @property
    def alg_relative_gap(self) -> float:
        """Reflects the gap used within the algorithm."""
        if self.lower_bound > 0:
            return self.upper_bound / self.lower_bound
        return float("inf")

    def alg_is_optimal(self, eps: float) -> bool:
        """Reflects the optimality criterion as used in the algorithm."""
        return self.alg_relative_gap <= (1 + eps)

    def self_plausibility_check(self, eps: float = 1e-3) -> dict[str, bool | str]:
        """Validate internal consistency of this solution.

        Checks that the solution's bounds, gap, trajectory, and flags are
        internally consistent without comparing to another solution.

        Args:
            eps: Numerical tolerance for floating point comparisons

        Returns:
            Dictionary with check results:
            - "valid": Overall validity (all checks passed)
            - "errors": List of error messages for failed checks
            - Individual check keys with True/False values

        Example:
            >>> result = solution.self_plausibility_check()
            >>> if not result["valid"]:
            ...     print(f"Errors: {result['errors']}")

        """
        import math

        errors = []
        checks = {}

        # Helper function to format values (handle inf)
        def fmt_val(val: float) -> str:
            return "inf" if math.isinf(val) else f"{val:.6f}"

        # Check 1: Bounds consistency
        # Allow inf for upper_bound, but LB <= UB must still hold
        checks["bounds_consistent"] = self.lower_bound <= self.upper_bound + eps or math.isinf(
            self.upper_bound
        )
        if not checks["bounds_consistent"]:
            errors.append(
                f"Bounds inconsistent: LB={fmt_val(self.lower_bound)} > "
                f"UB={fmt_val(self.upper_bound)}"
            )

        # Check 2: Gap calculation correctness (handle inf)
        expected_gap = self.upper_bound - self.lower_bound
        # If both gap and expected_gap are inf, that's ok
        if math.isinf(self.gap) and math.isinf(expected_gap):
            checks["gap_correct"] = True
        else:
            checks["gap_correct"] = abs(self.gap - expected_gap) <= eps
        if not checks["gap_correct"]:
            errors.append(
                f"Gap incorrect: stated={fmt_val(self.gap)}, calculated={fmt_val(expected_gap)}"
            )

        # Check 3: Trajectory length matches upper bound (skip if UB is inf)
        traj_length = self.trajectory.length
        if math.isinf(self.upper_bound):
            # If UB is inf, we can't check trajectory length
            checks["trajectory_matches_ub"] = True
        else:
            checks["trajectory_matches_ub"] = abs(traj_length - self.upper_bound) <= eps
        if not checks["trajectory_matches_ub"]:
            errors.append(
                f"Trajectory length mismatch: trajectory={fmt_val(traj_length)}, "
                f"UB={fmt_val(self.upper_bound)}"
            )

        # Check 4: Optimality flag consistency
        # If a solution claims to be optimal, gap should be small (and not inf)
        if self.is_optimal:
            # Can't be optimal with inf gap or UB
            if math.isinf(self.gap) or math.isinf(self.upper_bound):
                checks["optimality_flag_consistent"] = False
                errors.append(
                    "Optimality flag inconsistent: is_optimal=True but gap or upper_bound is inf"
                )
            else:
                # Most solvers use relative gap: gap <= eps * UB + eps (absolute)

                if "mip_gap" in self.statistics:
                    # Gurobi uses the relative mip gap
                    gap = (self.upper_bound - self.lower_bound) / self.upper_bound
                    checks["optimality_flag_consistent"] = gap <= (1 + eps)
                else:
                    ratio = self.upper_bound / self.lower_bound
                    checks["optimality_flag_consistent"] = ratio <= (1 + eps)
                if not checks["optimality_flag_consistent"]:
                    errors.append(
                        f"Optimality flag inconsistent: is_optimal=True but "
                        f"threshold={1 + eps} > "
                        f"gap={fmt_val(self.upper_bound / self.lower_bound)}"
                    )
        else:
            checks["optimality_flag_consistent"] = True

        # Check 5: Tour consistency
        if self.trajectory.is_empty:
            checks["tour_consistent"] = True
        else:
            checks["tour_consistent"] = self.is_tour == (
                abs(self.trajectory.coords[0][0] - self.trajectory.coords[-1][0]) <= eps
                and abs(self.trajectory.coords[0][1] - self.trajectory.coords[-1][1]) <= eps
            )
        if not checks["tour_consistent"]:
            errors.append(
                f"Tour flag inconsistent: is_tour={self.is_tour} but "
                f"trajectory endpoints don't match"
            )

        # Overall validity
        checks["valid"] = len(errors) == 0
        checks["errors"] = errors

        return checks

    def plausibility_check(
        self, other: "AnnotatedSolution", eps: float = 1e-3
    ) -> dict[str, bool | str]:
        """Perform plausibility checks comparing this solution with another.

        This method validates that bounds and properties are consistent across
        two solutions, which is useful when comparing solutions from different
        solvers or configurations for the same instance.

        Note: Handles infinite values for upper_bound and gap which can occur
        when no solution is found within the time limit.

        Args:
            other: Another AnnotatedSolution to compare against
            eps: Numerical tolerance for floating point comparisons

        Returns:
            Dictionary with check results:
            - "valid": Overall validity (all checks passed)
            - "errors": List of error messages for failed checks
            - Individual check keys with True/False values prefixed with "self_" or "other_"
            - Cross-instance check keys

        Example:
            >>> result = solution1.plausibility_check(solution2)
            >>> if not result["valid"]:
            ...     print(f"Errors: {result['errors']}")

        """
        import math

        errors = []
        checks = {}

        # Helper function to format values (handle inf)
        def fmt_val(val: float) -> str:
            return "inf" if math.isinf(val) else f"{val:.6f}"

        # Run self-checks for both solutions
        self_result = self.self_plausibility_check(eps)
        other_result = other.self_plausibility_check(eps)

        # Add self checks with "self_" prefix
        for key, value in self_result.items():
            if key == "errors":
                errors.extend(f"Self: {err}" for err in value)
            elif key != "valid":
                checks[f"self_{key}"] = value

        # Add other checks with "other_" prefix
        for key, value in other_result.items():
            if key == "errors":
                errors.extend(f"Other: {err}" for err in value)
            elif key != "valid":
                checks[f"other_{key}"] = value

        # Cross-instance checks
        # Check 1: Cross-instance bound consistency (handle inf)
        # The best lower bound from either solution should be <= all upper bounds
        best_lb = max(self.lower_bound, other.lower_bound)
        best_ub = min(self.upper_bound, other.upper_bound)

        # If best_ub is inf, the check always passes
        if math.isinf(best_ub):
            checks["cross_bounds_consistent"] = True
        else:
            checks["cross_bounds_consistent"] = best_lb <= best_ub + eps
        if not checks["cross_bounds_consistent"]:
            errors.append(
                f"Cross-instance bounds inconsistent: best_LB={fmt_val(best_lb)} > "
                f"best_UB={fmt_val(best_ub)}"
            )

        # Check 2: If both are optimal for the same instance, they should have similar bounds
        if self.is_optimal and other.is_optimal:
            # Both UBs should be similar (within eps tolerance), and not inf
            if math.isinf(self.upper_bound) or math.isinf(other.upper_bound):
                checks["optimal_solutions_agree"] = False
                errors.append("Both claim optimal but at least one has infinite upper_bound")
            else:
                checks["optimal_solutions_agree"] = bool(
                    np.isclose(self.upper_bound, other.upper_bound, rtol=eps)
                )
                if not checks["optimal_solutions_agree"]:
                    errors.append(
                        f"Both optimal but different values: "
                        f"self_UB={fmt_val(self.upper_bound)}, "
                        f"other_UB={fmt_val(other.upper_bound)}"
                    )
        else:
            checks["optimal_solutions_agree"] = True  # N/A if not both optimal

        # Overall validity
        checks["valid"] = len(errors) == 0
        checks["errors"] = errors

        return checks

    def check_feasibility(self, instance: AnnotatedInstance, eps: float = 0.01) -> bool:
        """Check if the solution trajectory visits all polygons."""
        return all(self.trajectory.intersects(polygon.buffer(eps)) for polygon in instance.polygons)
