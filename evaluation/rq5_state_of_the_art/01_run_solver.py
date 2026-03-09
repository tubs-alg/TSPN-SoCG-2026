import time
import zipfile
from pathlib import Path

import slurminade
from algbench import Benchmark
from tspn_bnb2.mip import generate_christofides_warm_start, solve_tspn_nonconvex_mip
from tspn_bnb2.operations import convert_to_tspn_instance
from tspn_bnb2.schemas import AnnotatedInstance

benchmark = Benchmark("results_mip_nonconvex_300", hide_output=False)
db_path = Path(__file__).parent.parent / "instances" / "instances_socg_simplified.zip"

time_limit = 300
max_points = 20

slurminade.update_default_configuration(
    partition="alg", constraint="alggen05", exclusive=True, mail_type="FAIL"
)
slurminade.set_dispatch_limit(200)


def run_mip_solver(_instance: AnnotatedInstance, alg_params: dict, **kwargs) -> dict:
    try:
        cpp_instance = convert_to_tspn_instance(_instance)

        # Generate initial solution and track time
        warmstart_start = time.time()
        initial_solution = generate_christofides_warm_start(cpp_instance)
        warmstart_time = time.time() - warmstart_start

        # Solve MIP with initial solution and track time
        mip_start = time.time()
        solution = solve_tspn_nonconvex_mip(
            cpp_instance,
            time_limit=time_limit,
            mip_gap=alg_params["mip_gap"],
            threads=24,
            verbose=False,
            initial_solution=initial_solution,
        )
        mip_time = time.time() - mip_start

    except RuntimeError as e:
        return {
            "solution": None,
            "error": f"RuntimeError: {e!s}",
        }
    except ValueError as e:
        return {
            "solution": None,
            "error": f"ValueError: {e!s}",
        }

    return {
        "solution": solution.model_dump_json(),
        "warmstart_time": warmstart_time,
        "warmstart_upper_bound": initial_solution.upper_bound,
        "mip_time": mip_time,
        "total_time": warmstart_time + mip_time,
    }


@slurminade.slurmify()
def load_instance_and_run(instance_path: str, alg_params: dict):
    """Load instance from zip file and run the MIP solver."""
    with zipfile.ZipFile(db_path, "r") as zf, zf.open(instance_path) as f:
        instance_json = f.read().decode("utf-8")
        instance = AnnotatedInstance.model_validate_json(instance_json)

    benchmark.add(
        run_mip_solver,
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
alg_params_to_evaluate = [{"mip_gap": mip_gap} for mip_gap in [0.001, 0.01, 0.05, 0.1, 0.15]]

if __name__ == "__main__":
    with slurminade.JobBundling(max_size=10):
        with zipfile.ZipFile(db_path, "r") as zf:
            instance_files = [name for name in zf.namelist() if name.endswith(".json")]
            print(f"Found {len(instance_files)} instances total")

            # Filter instances by size
            filtered_instances = []
            for instance_path in instance_files:
                with zf.open(instance_path) as f:
                    instance_json = f.read().decode("utf-8")
                    instance = AnnotatedInstance.model_validate_json(instance_json)
                    if len(instance.polygons) <= max_points:
                        filtered_instances.append(instance_path)

            print(f"Running on {len(filtered_instances)} instances with <= {max_points} points")
            for instance_path in filtered_instances:
                for conf in alg_params_to_evaluate:
                    load_instance_and_run.distribute(instance_path, conf)
        slurminade.join()
        compress.distribute()
