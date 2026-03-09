"""Extract simplified instances from benchmark results.

For each instance, this script:
1. First tries to get the simplified instance with all three simplifications enabled
   (remove_holes=True, convex_hull_fill=True, remove_supersites=True)
2. If that fails, tries with convex_hull_fill=False
   (remove_holes=True, convex_hull_fill=False, remove_supersites=True)
3. Saves the successfully simplified instances to instances_sm_simplified.zip
"""

import zipfile
from pathlib import Path

from algbench import read_as_pandas
from tspn_bnb2.schemas import AnnotatedInstance

instance_set = "socg"


def read_row(row):
    """Parse a benchmark row into a simplified format."""
    if row["result"]["annotated_instance"] is None:
        return None
    return {
        "annotated_instance": AnnotatedInstance.model_validate_json(
            row["result"]["annotated_instance"]
        ),
        "instance_name": row["parameters"]["args"]["kwargs"]["instance_name"],
        "instance": AnnotatedInstance.model_validate_json(
            row["parameters"]["args"]["kwargs"]["instance_json"]
        ),
        "simplification_time": row["result"]["simplification_time"],
        "remove_holes": row["parameters"]["args"]["alg_params"]["remove_holes"],
        "convex_hull_fill": row["parameters"]["args"]["alg_params"]["convex_hull_fill"],
        "remove_supersites": row["parameters"]["args"]["alg_params"]["remove_supersites"],
    }


def main():
    # Load benchmark results
    print("Loading benchmark results...")
    benchmark = read_as_pandas(f"results_simplification_{instance_set}", read_row)
    print(f"Loaded {len(benchmark)} simplification results")

    # Get unique instance names
    unique_instances = benchmark["instance_name"].unique()
    print(f"Found {len(unique_instances)} unique instances")

    # Extract simplified instances
    simplified_instances = {}
    failed_instances = []

    for instance_name in unique_instances:
        # Filter results for this instance
        instance_results = benchmark[benchmark["instance_name"] == instance_name]

        # Try to find result with all simplifications enabled
        all_true = instance_results[
            (instance_results["remove_holes"] == True)
            & (instance_results["convex_hull_fill"] == True)
            & (instance_results["remove_supersites"] == True)
        ]

        if len(all_true) > 0 and all_true.iloc[0]["annotated_instance"] is not None:
            # Success with all simplifications
            row = all_true.iloc[0]
            simplified_instances[instance_name] = row["annotated_instance"]
            continue

        # Try with convex_hull_fill=False
        convex_false = instance_results[
            (instance_results["remove_holes"] == True)
            & (instance_results["convex_hull_fill"] == False)
            & (instance_results["remove_supersites"] == True)
        ]

        if len(convex_false) > 0 and convex_false.iloc[0]["annotated_instance"] is not None:
            # Success with convex_hull_fill=False
            row = convex_false.iloc[0]
            simplified_instances[instance_name] = row["annotated_instance"]
            print(f"� {instance_name}: convex_hull_fill=False required")
            continue

        # Failed to simplify
        failed_instances.append(instance_name)
        print(f" {instance_name}: could not simplify")

    # Report statistics
    print("\n" + "=" * 60)
    print(f"Successfully simplified: {len(simplified_instances)}/{len(unique_instances)}")
    print(f"Failed: {len(failed_instances)}/{len(unique_instances)}")

    if failed_instances:
        print("\nFailed instances:")
        for name in failed_instances:
            print(f"  - {name}")

    # Save to zip file
    output_path = (
        Path(__file__).parent.parent / "instances" / f"instances_{instance_set}_simplified.zip"
    )
    print(f"\nSaving to {output_path}...")

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for instance_name, instance in simplified_instances.items():
            # Save as JSON with .json extension
            json_str = instance.model_dump_json(indent=2)
            zf.writestr(f"{instance_name}.json", json_str)

    print(f"Saved {len(simplified_instances)} simplified instances to {output_path}")


if __name__ == "__main__":
    main()
