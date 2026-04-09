"""Plotting utilities for paper figures (seaborn/LaTeX)."""

import matplotlib.pyplot as plt
import seaborn as sns

FIGWIDTH = 5.788  # inches
FULLWIDEFIGURE = (FIGWIDTH, 2)
HIGHFIGURE = (FIGWIDTH, 3)
HALFWIDEFIGURE = (2.8, 2)

MARGIN_WIDTH = 1.87831
TEXT_WIDTH = 4.2134
TEXT_FULL_WIDTH = 6.33585


def init_params() -> None:
    """Initialize matplotlib parameters for publication-quality figures."""
    sns.set_theme()

    plt.rcParams.update(
        {
            "text.usetex": True,
            # Font: Computer Modern (matches scrbook default)
            "font.family": "serif",
            "font.serif": ["Computer Modern Roman"],
            "text.latex.preamble": r"\usepackage{stmaryrd}",
            # Figure text sizes → \footnotesize
            "axes.labelsize": 8,
            "xtick.labelsize": 8,
            "ytick.labelsize": 8,
            "legend.fontsize": 7,  # slightly smaller to match visual size
            "legend.title_fontsize": 7,  # slightly smaller to match visual size
            # Titles → \small
            "axes.titlesize": 9,
            "figure.titlesize": 9,
        }
    )
