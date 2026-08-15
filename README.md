slugs - SmalL bUt Complete GROne Synthesizer
============================================

This repository is the CNU Robotics customized version of Slugs. It keeps the
upstream Slugs core while adding tooling, diagnostics, and experiment support
used by the
[CNU Robotics FlexBE synthesis toolchain](https://github.com/CNURobotics/flexbe_synthesis),
which depends on this Slugs fork.
See [CHANGELOG.md](CHANGELOG.md) for branch-specific changes, including
version/logging updates and helper-tool improvements. A key addition is the
experimental [well-separation analysis](#well-separation-analysis) mode for
diagnosing GR(1) environment assumptions.

----

Slugs is a stand-alone reactive synthesis tool for generalized reactivity(1) synthesis. It uses binary decision diagrams (BDDs) as the primary data structure for efficient symbolic reasoning. 

If you want to cite slugs in a scientific paper, please cite its tool paper:

- Rüdiger Ehlers and Vasumathi Raman: _Slugs: Extensible GR(1) Synthesis_. 28th International Conference on Computer Aided Verification (CAV 2016), Volume 2, p.333-339

You can find an author-archived version of the paper [here](https://www.ruediger-ehlers.de/papers/cav2016.pdf). The paper has an appendix that contains an introduction to using slugs and its input language.

The slugs distribution comes with the CUDD library for manipulating binary decision diagrams (BDDs), written by Fabio Somenzi. Please see the README and LICENSE files in the `lib/cudd-3.0.0` folder for details. The _dddmp_ library for loading and saving BDDs that comes with CUDD has other licensing terms than CUDD that permit only academic and educational use. Please consult the source files in the `lib/cudd-3.0.0/dddmp` folder for details.

An introduction video to reactive synthesis with a focus on generalized reactivity(1) synthesis [here](https://www.ruediger-ehlers.de/blog/introtoreactivesynthesis.html). It also contains a Slugs tool demo.


Installation
============

Requirements
------------
- A moderately modern C++ and C compiler installed in a Unix-like environment, including the C++ library boost. Linux and MacOS should be fine.
- An installation of Python 2, version 2.7 or above. The Python _curses_ library must be installed for the interactive specification debugger to be usable.

Using Slugs on Linux
-------------------
In order to build slugs, open a terminal and type:

> cd src; make

The slugs executable will be put into the src directory.

Using slugs on OS X
-------------------
Things should generally work fine if you have a package management system (i.e. Homebrew or Macports) installed and follow the above instructions for Linux.
Note that you will need to:

- have [`gcc, g++`](https://gcc.gnu.org/) in your `$PATH`,
- have installed at least the package: `boost`.

Well-separation analysis
========================

This fork includes an experimental GR(1) well-separation analysis mode for
compiled `.slugsin` specifications:

> src/slugs --checkWellSeparation examples/well_separation/forklift_cargo_conflict.slugsin

The result is emitted as JSON on standard output. To write JSON to a file:

> src/slugs --checkWellSeparation --jsonOutput examples/well_separation/forklift_cargo_conflict.slugsin result.json

For slower opt-in core diagnostics, add:

> src/slugs --checkWellSeparation --minimizeWellSeparationCore examples/well_separation/forklift_cargo_conflict.slugsin

The JSON includes a pre-release `format_version` currently set to `0.1`, tool
metadata, status, Maoz/Ringert case labels, category-level responsible
assumptions with source indexes and line numbers, and a representative
state-cube witness when available. Witnesses include compact `details` metadata
for one-step safety violations or abstract liveness traps. Assumption names are
read from `##` comments immediately before environment assumption clauses.
When `--minimizeWellSeparationCore` is used, `core_assumptions` reports a
1-minimal non-well-separated subset found via delta debugging (Zeller's
DDMin), the same search strategy used by the Maoz/Ringert and Gorenstein/
Maoz/Ringert reference implementations this mode is validated against; this
is not guaranteed to be a globally smallest core.

Smoke fixtures can be checked with:

> SLUGS_BINARY=src/slugs tools/testWellSeparation.py

The tiny fixtures can also be cross-checked against an explicit-state oracle:

> SLUGS_BINARY=src/slugs tools/testWellSeparationOracle.py

Current limitations:

- The mode analyzes plain compiled `.slugsin` semantics.
- Auxiliary variables introduced by higher-level specification compilers are not
  separated from ordinary environment assumptions.
- `responsible_assumptions` and witness `implicated_assumptions` are
  diagnostics, not minimized non-well-separated cores.
- `core_assumptions`, when requested, are 1-minimal (via delta debugging) and
  can require many additional BDD fixed-point calls.
- The current witness is a representative state cube, not a full strategy,
  counterstrategy, or trace.

A short primer on the internal structure of slugs
=================================================

The structure of slugs is documented in a doxygen compatible format. Internally, it uses the 'BFAbstractionLibrary' - It handles all calls to the BDD library, but allows to easily replace the BDD library actually used. For using the BFAbstractionLibrary, it suffices to include 'BF.h' - the choice of concrete BDD library used is done via preprocessor directives (see 'BF.h'). The 'BF' in 'BFAbstractionLibrary' stands for 'Boolean function'.

That library has a couple of classes:

- BF - The actual Boolean function. Supports operations like AND, OR, ..., using C++ operator overloading
- BFManager - It keeps a list of BF variables, provided TRUE or FALSE constants, and can compute variable vectors or cubes
- BFVarCube - An unsorted list of variables - used for existentially or universally quantifying out variables from BDDs
- BFVarVector - A sorted list of variables - used for replacing variables in BFs by other variables

There is also a library component for dumping BDDs - this is useful for debugging. For this to work, the application has to provide the dumping function with information about string names for BDD variable and the like. The 'Slugs' main container class inherits the class 'VariableInfoContainer' for this purpose. 

The actual synthesis part is built around a class that is named "GR1Context". It is explained in the automatically generated doxygen documentation. The class GR1Context is the main class that can be inherited for modifications of the synthesis algorithm. The 'main' function in the file 'main.cpp' is concerned with building the proper context class for synthesis and running the synthesis algorithm then.
