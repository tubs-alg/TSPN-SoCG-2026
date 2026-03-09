import time
import zipfile
from pathlib import Path

import slurminade
from algbench import Benchmark
from tspn_bnb2.operations import simplify_annotated_instance
from tspn_bnb2.order_annotation import add_order_annotations
from tspn_bnb2.schemas import AnnotatedInstance

instance_set = "socg"

benchmark = Benchmark(f"results_simplification_{instance_set}", hide_output=False)
db_path = Path(__file__).parent.parent / "instances" / f"instances_{instance_set}.zip"

slurminade.update_default_configuration(
    partition="alg", constraint="alggen05", exclusive=True, mail_type="FAIL"
)
slurminade.set_dispatch_limit(200)


def run_simplify_annotated_instance(
    _instance: AnnotatedInstance, alg_params: dict, **kwargs
) -> dict:
    start_time = time.time()
    try:
        output_instance = simplify_annotated_instance(
            _instance,
            remove_holes=alg_params["remove_holes"],
            convex_hull_fill=alg_params["convex_hull_fill"],
            remove_supersites=alg_params["remove_supersites"],
        )
    except RuntimeError:
        return {
            "output_instance": None,
            "annotated_instance": None,
            "simplification_time": None,
            "annotation_time": None,
            "error": "RuntimeError during simplification",
        }
    end_time = time.time()
    simplification_time = end_time - start_time

    start_time = time.time()
    annotated_instance = add_order_annotations(output_instance)
    end_time = time.time()
    annotation_time = end_time - start_time

    return {
        "output_instance": output_instance.model_dump_json(),
        "simplification_time": simplification_time,
        "annotation_time": annotation_time,
        "annotated_instance": annotated_instance.model_dump_json(),
    }


@slurminade.slurmify()
def load_instance_and_run(instance_path: str, alg_params: dict):
    """Load instance from zip file and run the simplification."""
    with zipfile.ZipFile(db_path, "r") as zf, zf.open(instance_path) as f:
        instance_json = f.read().decode("utf-8")
        instance = AnnotatedInstance.model_validate_json(instance_json)

    benchmark.add(
        run_simplify_annotated_instance,
        instance_name=Path(instance_path).stem,
        instance_json=instance.model_dump_json(),
        alg_params=alg_params,
        _instance=instance,
    )


@slurminade.slurmify(mail_type="ALL")
def compress():
    """Compress benchmark results."""
    benchmark.compress()


# Algorithm configurations to evaluate
alg_params_to_evaluate = [
    {
        "remove_holes": True,
        "convex_hull_fill": False,
        "remove_supersites": True,
    },
    {
        "remove_holes": True,
        "convex_hull_fill": True,
        "remove_supersites": True,
    },
]

if __name__ == "__main__":
    with slurminade.JobBundling(max_size=100):  # combine up to 100 calls into one task
        with zipfile.ZipFile(db_path, "r") as zf:
            instance_files = [name for name in zf.namelist() if name.endswith(".json")]
            print(f"Found {len(instance_files)} instances")
            for instance_path in instance_files:
                for conf in alg_params_to_evaluate:
                    load_instance_and_run.distribute(instance_path, conf)
        slurminade.join()
        compress.distribute()
