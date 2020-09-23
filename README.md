### Overview

![Logo](http://shcherbyna.com/files/igel.bmp)

[![Build Status](https://api.travis-ci.org/vshcherbyna/igel.svg?branch=master)](https://travis-ci.org/vshcherbyna/igel)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/vshcherbyna/igel?svg=true)](https://ci.appveyor.com/project/vshcherbyna/igel)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/vshcherbyna/igel.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/vshcherbyna/igel/alerts/)

Igel is a free UCI chess engine forked from GreKo 2018.01. It is not a complete chess program and requires some UCI compatible GUI software in order to be used.

### History

Igel started as a hobby project in early 2018 to learn chess programming. The name 'Igel' is a German translation of 'Hedgehog' and was chosen to represent numerios hedgehogs living in my garden.

Igel was forked from GreKo 2018.01 and the main reason for the fork was to study the existing chess engine and improve its strength over time and learn new things. GreKo was chosen because it had a clean code, it supported Visual Studio and it was not a very strong engine so further improvements would be possible.

The first versions of Igel were actually regressions and had less strength than the original version of GreKo that were used to fork Igel. After trying a few things and lacking any experience in chess engine development the work on Igel was halted by late 2018.

In March 2019 Igel was invited to a prestigious chess tournament for top chess engines - TCEC to participate in season 15 and it took last place in Division 4a. Seeing poor performance of Igel it was a great motivation factor to improve the engine and active development work has begun. By late 2019 Igel had surpassed 3000 elo in CCRL Blitz and entered the top 50 engines in CCRL list.

By mid 2020 Igel 2.5.0 64-bit 4CPU reached 3245 elo in CCRL Blitz on 4CPU and entered the top 30 engines of the list.

In June 2020 Igel was invited by Andrew Grant to participate in OpenBench testing framework and this has further accelerated the strength improvement of the engine. In August 2020 Igel switched to NNUE as a main evaluation function using Dietrich Kappe's NiNu network file and is currently approaching the top ten strongest chess engines on CCRL list.

### Acknowledgements

I would like to thank the authors and the community involved in the creation of the open source projects listed below. Their work influences development of Igel, and without them, this project wouldn't exist. Special thanks to Andrew Grant and Bojun Guo for supporting Igel development on OpenBench.

* [OpenBench](https://github.com/AndyGrant/OpenBench/)
* [GreKo](http://greko.su/)
* [Chess Programming Wiki](https://www.chessprogramming.org/)
* [Ethereal](https://github.com/AndyGrant/Ethereal/)
* [Xiphos](https://github.com/milostatarevic/xiphos/)
* [Stockfish](https://github.com/official-stockfish/Stockfish/)
* [Fathom](https://github.com/jdart1/Fathom/)
* [Syzygy](https://github.com/syzygy1/tb)
* Yu Nasu for creating NNUE and Hisayori Noda and others for integrating it in Stockfish
* Dietrich Kappe for creating NiNu network and allowing it to use in Igel releases

### Compiling

Official compilation method involves CMake and Visual Studio 2019 and assumes a modern CPU with AVX2 support (most of the computers produced in last 8 years).

```
cmake -DEVAL_NNUE=1 -DUSE_AVX2=1 -D_BTYPE=1 -DSYZYGY_SUPPORT=TRUE -G "Visual Studio 16 2019" -A x64 .
```

In case you don't have Visual Studio or CMake you can compile using gcc as well:

```
g++ -Wall -pthread -O3 -DEVAL_NNUE=1 -DUSE_AVX2=1 -DSYZYGY_SUPPORT=TRUE -march=native -flto *.cpp fathom/tbprobe.c nnue/*.cpp nnue/features/*.cpp -D_BTYPE=1 -DNDEBUG -o igel
```