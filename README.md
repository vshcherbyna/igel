### Overview

![Logo](https://raw.githubusercontent.com/vshcherbyna/igel/master/igel.bmp)

[![Build Status](https://ci.appveyor.com/api/projects/status/github/vshcherbyna/igel?svg=true)](https://ci.appveyor.com/project/vshcherbyna/igel)

Igel is a free UCI chess engine from Ukraine. It is not a complete chess program and requires some UCI compatible GUI software in order to be used.

### History

Igel started as a hobby project in early 2018 to learn chess programming. The name 'Igel' is a German translation of 'Hedgehog' and was chosen to represent numerios hedgehogs living in my garden.

Igel was forked from GreKo 2018.01 and the main reason for the fork was to study the existing chess engine and improve its strength over time and learn new things. GreKo was chosen because it had a clean code, it supported Visual Studio and it was not a very strong engine so further improvements would be possible.

The first versions of Igel were actually regressions and had less strength than the original version of GreKo that were used to fork Igel. After trying a few things and lacking any experience in chess engine development the work on Igel was halted by late 2018.

In March 2019 Igel was invited to a prestigious chess tournament for top chess engines - TCEC to participate in season 15 and it took last place in Division 4a. Seeing poor performance of Igel it was a great motivation factor to improve the engine and active development work has begun. By late 2019 Igel had surpassed 3000 elo in CCRL Blitz and entered the top 50 engines in CCRL list.

By mid 2020 Igel 2.5.0 64-bit 4CPU reached 3245 elo in CCRL Blitz on 4CPU and entered the top 30 engines of the list.

In June 2020 Igel was invited by Andrew Grant to participate in OpenBench testing framework and this has further accelerated the strength improvement of the engine.

In August 2020 Igel experimened with NNUE using Dietrich Kappe's NiNu network file in it's initial releases.

As of late 2020 Igel adoped NNUE and uses own NNUE implementation and own network file trained on Igel data.

In January 2023 last bits of HCE code were removed from Igel and the evaluation is fully based on NNUE as of Igel 3.4.0.

### Acknowledgements

I would like to thank the authors and the community involved in the creation of the open source projects listed below. Their work influences development of Igel, and without them, this project wouldn't exist. Special thanks to Andrew Grant and Bojun Guo for supporting Igel development on OpenBench.

* [OpenBench](https://github.com/AndyGrant/OpenBench/)
* [nnue-pytorch](https://github.com/glinscott/nnue-pytorch)
* [GreKo](http://greko.su/)
* [Chess Programming Wiki](https://www.chessprogramming.org/)
* [Ethereal](https://github.com/AndyGrant/Ethereal/)
* [Xiphos](https://github.com/milostatarevic/xiphos/)
* [Stockfish](https://github.com/official-stockfish/Stockfish/)
* [Fathom](https://github.com/jdart1/Fathom/)
* [Syzygy](https://github.com/syzygy1/tb)
* [Dietrich Kappe](https://www.patreon.com/badgyal) for creating Night Nurse network and allowing it to use in Igel 2.7.0 and 2.8.0 releases
* [Dietrich Kappe](https://www.patreon.com/badgyal) for sharing his knowledge/tooling for NNUE networks training
* Yu Nasu for creating NNUE and Hisayori Noda and others for integrating it in Stockfish

### Compiling

Official compilation method involves cmake and gcc/Visual Studio 2019 and assumes a modern CPU with AVX2 support (most of the computers produced in last 8 years).

Using cmake/Visual Studio 2022:

```
git clone https://github.com/vshcherbyna/igel.git ./igel
cd igel
git submodule update --init --recursive
cmake -DUSE_AVX2=1 -D_BTYPE=1 -DSYZYGY_SUPPORT=TRUE -G "Visual Studio 17 2022" -A x64 .
```

Using cmake/gcc:

```
git clone https://github.com/vshcherbyna/igel.git ./igel
cd igel
git submodule update --init --recursive
wget https://github.com/vshcherbyna/igel/releases/download/3.5.0/c049c117 -O ./network_file
cmake -DEVALFILE=network_file -DUSE_PEXT=1 -DUSE_AVX2=1 -D_BTYPE=1 -DSYZYGY_SUPPORT=TRUE .
make -j
```

Important! If you make a custom build of Igel you need to validate the bench using command:

```
igel.exe bench
```

or if you are running Linux:

```
./igel bench
```

The 'Nodes' must match the 'BENCH :' value from the last commit message.

It is also possible to compile using gcc and a traditional makefile, please consult ./src/makefile for more details.

### Donation

Thank you so much for supporting Igel's development and training via [![Paypal](https://raw.githubusercontent.com/vshcherbyna/igel/master/paypal3.jpg)](https://www.paypal.com/donate/?hosted_button_id=37NPEAY9XLRJE).