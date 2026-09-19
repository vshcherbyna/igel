### Overview

![Logo](https://raw.githubusercontent.com/vshcherbyna/igel/master/igel.bmp)

Igel is a free UCI chess engine from Ukraine. It is not a complete chess program: it needs a UCI-compatible GUI to be used.

### History

Igel began as a hobby project in early 2018, as a way to learn chess programming. 'Igel' is German for 'hedgehog', and the name was chosen after the many hedgehogs living in my garden.

Igel was forked from GreKo 2018.01. The aim of the fork was to study an existing chess engine, improve its strength over time and learn along the way. GreKo was chosen because it had clean code, it supported Visual Studio, and it was not yet very strong, which left plenty of room for improvement.

The first versions of Igel were in fact regressions: they were weaker than the version of GreKo they had been forked from. After trying a few ideas, and with no experience in chess engine development, I halted the work in late 2018.

In March 2019 Igel was invited to TCEC, a prestigious tournament for top chess engines, to take part in season 15, where it finished last in Division 4a. That result turned out to be a strong motivation to improve the engine, and active development began. By late 2019 Igel had passed 3000 Elo in CCRL Blitz and entered the top 50 of the list.

By mid-2020, Igel 2.5.0 64-bit reached 3245 Elo on four CPUs in CCRL Blitz and entered the top 30 of the list.

In June 2020 Andrew Grant invited Igel to the OpenBench testing framework, which accelerated the engine's progress considerably.

In August 2020 Igel experimented with NNUE, using Dietrich Kappe's NiNu network in its first NNUE releases.

Since late 2020 Igel has used its own NNUE implementation, with its own network trained on Igel data.

In January 2023 the last of the hand-crafted evaluation was removed from the default build, and from Igel 3.4.0 the evaluation is entirely NNUE. A pure HCE build was later restored for TCEC and is still available through `-DPURE_HCE`.

### Evaluation

- NNUE
  - Horizontally Mirrored 32 King Buckets
  - HalfKAv2_hm + FullThreats
  - 2x(22528 + 60144 -> 1024) -> 16 -> 32 -> 1
  - 8 Output Buckets
  - 8 PSQT Buckets
  - Using Igel evaluation and search data
- Igel HCE (-DPURE_HCE)

### Acknowledgements

I would like to thank the authors and the communities behind the open source projects listed below. Their work has shaped the development of Igel, and without them this project would not exist. Special thanks to Andrew Grant and Bojun Guo for supporting Igel development on OpenBench.

* [OpenBench](https://github.com/AndyGrant/OpenBench/)
* [nnue-pytorch](https://github.com/glinscott/nnue-pytorch)
* [GreKo](http://greko.su/)
* [Chess Programming Wiki](https://www.chessprogramming.org/)
* [Ethereal](https://github.com/AndyGrant/Ethereal/)
* [Xiphos](https://github.com/milostatarevic/xiphos/)
* [Stockfish](https://github.com/official-stockfish/Stockfish/)
* [Fathom](https://github.com/jdart1/Fathom/)
* [Syzygy](https://github.com/syzygy1/tb)
* [Dietrich Kappe](https://www.patreon.com/badgyal) for creating the Night Nurse network and allowing its use in the Igel 2.7.0 and 2.8.0 releases
* [Dietrich Kappe](https://www.patreon.com/badgyal) for sharing his knowledge and tooling for NNUE network training
* Yu Nasu for creating NNUE, and Hisayori Noda and others for integrating it into Stockfish

### Compiling

Igel can be built either with the makefile in `src/` or with CMake. Both need a C++17 compiler and a CPU with AVX2 support, which means Intel Haswell (2013) or AMD Zen (2017) and later.

Start by cloning the repository together with its submodules:

```
git clone https://github.com/vshcherbyna/igel.git ./igel
cd igel
git submodule update --init --recursive
```

#### Using the makefile

This is how the released binaries and the CI builds are produced. The `pgo` target applies
profile-guided optimisation, which gives the fastest binary, and it downloads the network for you
the first time it runs:

```
cd src
make pgo
```

It needs Clang and the matching LLVM tools. The makefile expects them under their versioned names
(`clang++-19`, `lld-19`, `llvm-profdata-19`); where they are installed without a version suffix, as
in an MSYS2 CLANG64 environment on Windows, name them explicitly. Override `CC` there as well: the
makefile runs it to discover which instruction sets your CPU supports, and a CLANG64 environment has
no `g++` unless you install one, which would leave the build without AVX2:

```
mingw32-make pgo CC=clang++ CLANGCC=clang++ CLANGLD=lld PROFDATA=llvm-profdata
```

To build against the Fischer Random network instead of the standard one, name `frc` alongside the
build target:

```
make pgo frc
```

Two other targets are available. `make pgo_hce` builds the hand-crafted evaluator, which needs no
network at all. `make basic` is a plain GCC build without profile-guided optimisation; it downloads
nothing, so point it at a network you already have, for example `make basic EVALFILE=../network_file`.

#### Using CMake

On GCC and Clang the network is embedded into the binary at compile time, so CMake has to be told
which file to use:

```
wget https://github.com/vshcherbyna/igel/releases/download/0.8/6b953e78.standard -O ./network_file
cmake -DEVALFILE=network_file -DUSE_AVX2=1 -DSYZYGY_SUPPORT=TRUE .
make -j
```

A Visual Studio build does **not** embed the network. It opens a file named `network_file` in the
working directory when it starts, so download that file and keep it next to `igel.exe`:

```
curl -L -o network_file https://github.com/vshcherbyna/igel/releases/download/0.8/6b953e78.standard
cmake -DUSE_AVX2=1 -DSYZYGY_SUPPORT=TRUE -G "Visual Studio 17 2022" -A x64 .
cmake --build . --config Release
```

The options CMake understands:

| Option | Meaning |
|---|---|
| `-DEVALFILE=<file>` | Network to embed. Required for GCC and Clang builds. Visual Studio ignores it and reads `network_file` at startup instead |
| `-DUSE_AVX2=1` | AVX2, the baseline for every released binary |
| `-DUSE_AVX512=1` | AVX-512. Add `-DUSE_VNNI=1` for VNNI-512 |
| `-DUSE_AVXVNNI=1` | AVX-VNNI, the 256-bit form |
| `-D_BTYPE=1` | Use `pext` for sliding piece lookups. This is faster on Intel Haswell and later and on AMD Zen 3 and later, but **slower on Zen 1 and Zen 2**, which emulate `pext` in microcode, so leave it out on those. The makefile detects this on its own |
| `-DSYZYGY_SUPPORT=TRUE` | Syzygy endgame tablebase support |
| `-DPURE_HCE=1` | Build the hand-crafted evaluator instead of NNUE. Needs no network |

#### Checking your build

Any build you make yourself should be checked against the reference node count:

```
igel bench
```

on Windows, or:

```
./igel bench
```

on Linux. The `Nodes` figure it prints must match the `bench:` value in the latest commit message.
If the two differ, your binary is not searching the same tree as the reference build, and any result
it produces cannot be compared with published ones.