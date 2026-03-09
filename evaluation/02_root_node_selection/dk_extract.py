# %%
from algbench import read_as_pandas
from tspn_bnb2.plotting import init_params
from tspn_bnb2.schemas import AnnotatedInstance, AnnotatedSolution

init_params()
# %%
map_root_name = {
    "LongestEdgePlusFurthestSite": "LEFP",
    "RandomRoot": "Random",
    "OrderRoot": "CHR",
    "LongestPair": "LP",
}


# %%
def read_row(row):
    instance = AnnotatedInstance.model_validate_json(
        row["parameters"]["args"]["kwargs"]["instance_json"]
    )
    solution = AnnotatedSolution.model_validate_json(row["result"]["solution"])
    return {
        "solution": solution,
        "upper_bound": solution.upper_bound,
        "lower_bound": solution.lower_bound,
        "is_optimal": solution.is_optimal,
        "instance_name": row["parameters"]["args"]["kwargs"]["instance_name"],
        "instance": instance,
        "solve_time": row["result"]["solve_time"],
        "n": instance.num_polygons(),
        "root_strategy": map_root_name[row["parameters"]["args"]["alg_params"]["root"]],
        "instance_type": instance.derive_instance_type(),
    }


benchmark = read_as_pandas("results_root_strategy", read_row)
benchmark = benchmark.sort_values(by=["root_strategy"])
print(
    "loaded",
    len(benchmark),
    "runs",
    benchmark["root_strategy"].unique(),
    len(benchmark["instance_name"].unique()),
)
# %%
# validate that solutions are correct.
n_checks = 0
for _, row in benchmark.iterrows():
    solution = row["solution"]
    if solution is None:
        continue
    same_instance = benchmark[benchmark["instance_name"] == row["instance_name"]]

    for _, other in same_instance.iterrows():
        if other["solution"] is None:
            continue
        check = solution.plausibility_check(other["solution"], eps=0.01)
        assert check["valid"], (
            f"Check failed for {row['instance_name']} {check}"
            f" {other['solution']} {row['root_strategy']}"
        )
        n_checks += 1

print(n_checks, "checks succeeded")
# %%
# Describe the data we have in the table
df = benchmark[
    [
        "instance_name",
        "instance_type",
        "upper_bound",
        "lower_bound",
        "solve_time",
        "n",
        "root_strategy",
    ]
]
# %%
df.to_csv("/home/krupke/Repositories/tspn/dominiks_data_analysis/root_node/root_node_data.csv")

# %%
