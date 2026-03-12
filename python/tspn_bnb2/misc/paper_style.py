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
            "font.size": 7,
            "axes.titlesize": 7,
            "axes.labelsize": 7,
            "legend.fontsize": 7,
            "figure.titlesize": 7,
            "xtick.labelsize": 7,
            "ytick.labelsize": 7,
            "legend.title_fontsize": 7,
            "figure.figsize": FULLWIDEFIGURE,
            "savefig.bbox": "tight",
            "savefig.pad_inches": 0.01,
        }
    )
