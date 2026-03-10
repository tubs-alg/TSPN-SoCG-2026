"""Scikit-Build instruction file.

This file instructs scikit-build how to build the module. This is very close
to the classical setuptools, but wraps some things for us to build the native
modules easier and more reliable.

For a proper installation with `pip install .`, you additionally need a
`pyproject.toml` to specify the dependencies to load this `setup.py`.

You can use `python3 setup.py install` to build and install the package
locally, with verbose output. To build this package in place, which may be
useful for debugging, use `python3 setup.py develop`. This will build
the native modules and move them into your source folder.

The setup options are documented here:
https://scikit-build.readthedocs.io/en/latest/usage.html#setup-options
"""

from pathlib import Path

from setuptools import find_packages
from skbuild_conan import setup


def readme() -> str:
    """Simply return the README.md as string."""
    with Path("README.md").open() as file:
        return file.read()


setup(  # https://scikit-build.readthedocs.io/en/latest/usage.html#setup-options
    # ~~~~~~~~~ BASIC INFORMATION ~~~~~~~~~~~
    name="tspn-bnb2",
    version="0.2.1",  # Must match tspn_bnb2.__version__ (verified by test)
    # ~~~~~~~~~~~~ CRITICAL PYTHON SETUP ~~~~~~~~~~~~~~~~~~~
    # This project structures defines the python packages in a subfolder.
    # Thus, we have to collect this subfolder and define it as root.
    packages=find_packages("python"),  # Include all packages in `./python`.
    package_dir={"": "python"},  # The root for our python package is in `./python`.
    python_requires=">=3.12",  # lowest python version supported.
    install_requires=[
        # requirements necessary for basic usage (subset of requirements.txt)
        "chardet>=4.0.0",
        "networkx>=2.5.1",
        "requests>=2.25.1",
        "shapely~=2.1.2",
        "pydantic~=2.12.5",
    ],
    # ~~~~~~~~~~~ CRITICAL CMAKE SETUP ~~~~~~~~~~~~~~~~~~~~~
    # Especially LTS systems often have very old CMake version (or none at all).
    # Defining this will automatically install locally a working version.
    cmake_minimum_required_version="3.23",
    #
    # By default, the `install` target is built (automatically provided).
    # To compile a specific target, use the following line.
    # Alternatively, you can use `if(NOT SKBUILD) ... endif()` in CMake, to
    # remove unneeded parts for packaging (like tests).
    # cmake_install_target = "install"
    #
    # In the cmake you defined by install(...) where to move the built target.
    # This is critical als only targets with install will be used by skbuild.
    # This should be relative paths to the project root, as you don't know
    # where the package will be packaged. You can change the root for the
    # install-paths with the following line. Note that you can also access
    # the installation root (including this modification) in cmake via
    # `CMAKE_INSTALL_PREFIX`. If your package misses some binaries, you
    # probably messed something up here or in the `install(...)` path.
    # cmake_install_dir = ".",
    # |-----------------------------------------------------------------------|
    # | If you are packing foreign code/bindings, look out if they do install |
    # | targets in global paths, like /usr/libs/. This could be a problem.    |
    # |-----------------------------------------------------------------------|
    #
    # Some CMake-projects allow you to configure it using parameters. You
    # can specify them for this Python-package using the following line.
    # cmake_args=[]
    # There are further options, but you should be fine with these above.
    # ~~~~~~~~ Conan ~~~~~~~~~~~~~
    conan_recipes=["./cmake/conan/gurobi_public/"],
    conan_requirements=[
        "nlohmann_json/[>=3.11]",
        "gurobi/[>=12.0.0]",
        "cgal/[>=6.0.1]",
        "boost/[>=1.83.0]",
        "mpfr/[>=4.2.1]",
        "fmt/[>=11.0.0]",
        "gtest/[>=1.14]",
        "pybind11/[>=2.13.6]",
    ],
)
