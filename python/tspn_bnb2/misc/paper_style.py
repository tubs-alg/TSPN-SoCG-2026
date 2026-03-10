"""Plotting utilities for paper figures (seaborn/LaTeX)."""

import matplotlib.pyplot as plt
import seaborn as sns

FIGWIDTH = 5.788  # inches
FULLWIDEFIGURE = (FIGWIDTH, 2)
HIGHFIGURE = (FIGWIDTH, 3)
HALFWIDEFIGURE = (2.8, 2)


def init_params() -> None:
    """Initialize matplotlib parameters for publication-quality figures."""
    sns.set_theme(context="notebook", style="whitegrid")
    plt.rcParams.update(
        {
            "text.usetex": True,
            "text.latex.preamble": " \\usepackage{lmodern}",
            "font.family": ["Latin Modern Roman"],
            "font.size": 9,
            "axes.titlesize": 9,
            "axes.labelsize": 9,
            "legend.fontsize": 9,
            "figure.titlesize": 9,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
            "legend.title_fontsize": 9,
            "figure.figsize": FULLWIDEFIGURE,
            "savefig.bbox": "tight",
            "savefig.pad_inches": 0.01,
        }
    )
