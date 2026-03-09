import time
import zipfile
from pathlib import Path

import slurminade
from algbench import Benchmark
from tspn_bnb2.operations import solve_annotated_instance
from tspn_bnb2.schemas import AnnotatedInstance

benchmark = Benchmark("results_search_strategy", hide_output=False)
db_path = Path(__file__).parent.parent / "instances" / "instances_socg_60_simplified.zip"
# db_path = Path(__file__).parent.parent / "instances" / "test.zip"

time_limit = 60

slurminade.update_default_configuration(
    partition="alg", constraint="alggen05", exclusive=True, mail_type="FAIL"
)
slurminade.set_dispatch_limit(200)


def run_solve_with_search_strategy(
    _instance: AnnotatedInstance, alg_params: dict, **kwargs
) -> dict:
    start_time = time.time()
    try:
        solution = solve_annotated_instance(
            _instance,
            timelimit=time_limit,
            search=alg_params["search"],
            root="OrderRoot",
            num_threads=24,
            decomposition_branch=True,
            eps=alg_params["eps"],
            callback=None,
            initial_solution=None,
        )
    except RuntimeError as e:
        return {
            "solution": None,
            "error": f"RuntimeError: {e!s}",
        }
    except ValueError as e:
        return {
            "solution": None,
            "error": f"Value Error: {e!s}",
        }

    end_time = time.time()
    total_time = end_time - start_time

    return {
        "solution": solution.model_dump_json(),
        "solve_time": total_time,
    }


@slurminade.slurmify()
def load_instance_and_run(instance_path: str, alg_params: dict):
    """Load instance from zip file and run the solver with specified search strategy."""
    with zipfile.ZipFile(db_path, "r") as zf, zf.open(instance_path) as f:
        instance_json = f.read().decode("utf-8")
        instance = AnnotatedInstance.model_validate_json(instance_json)

    benchmark.add(
        run_solve_with_search_strategy,
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
    item
    for eps in [0.01, 0.05]
    for item in [
        {
            "search": "DfsBfs",
            "eps": eps,
        },
        {"search": "CheapestChildDepthFirst", "eps": eps},
        {"search": "CheapestBreadthFirst", "eps": eps},
    ]
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
