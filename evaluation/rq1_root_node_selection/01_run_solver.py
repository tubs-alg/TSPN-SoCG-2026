import time
import zipfile
from pathlib import Path

import slurminade
from algbench import Benchmark
from tspn_bnb2.operations import solve_annotated_instance
from tspn_bnb2.schemas import AnnotatedInstance

benchmark = Benchmark("results_root_strategy", hide_output=False)
db_path = Path(__file__).parent.parent / "instances" / "instances_socg_60_simplified.zip"
# db_path = Path(__file__).parent.parent / "instances" / "test.zip"

time_limit = 60

slurminade.update_default_configuration(
    partition="alg", constraint="alggen05", exclusive=True, mail_type="FAIL"
)
slurminade.set_dispatch_limit(200)


def run_simplify_annotated_instance(
    _instance: AnnotatedInstance, alg_params: dict, **kwargs
) -> dict:
    start_time = time.time()
    try:
        solution = solve_annotated_instance(
            _instance,
            timelimit=time_limit,
            root=alg_params["root"],
            num_threads=24,
            eps=0.01,
            decomposition_branch=True,
            callback=None,
            initial_solution=None,
        )
    except RuntimeError:
        return {
            "solution": None,
            "error": "RuntimeError during simplification",
        }
    end_time = time.time()
    total_time = end_time - start_time

    return {
        "solution": solution.model_dump_json(),
        "solve_time": total_time,
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
    {"root": "LongestEdgePlusFurthestSite"},
    {"root": "RandomRoot"},
    {"root": "LongestPair"},
    {"root": "OrderRoot"},
]

if __name__ == "__main__":
    with slurminade.JobBundling(max_size=10):
        with zipfile.ZipFile(db_path, "r") as zf:
            instance_files = [name for name in zf.namelist() if name.endswith(".json")]
            print(f"Found {len(instance_files)} instances")
            for instance_path in instance_files:
                for conf in alg_params_to_evaluate:
                    load_instance_and_run.distribute(instance_path, conf)
        slurminade.join()
        compress.distribute()
