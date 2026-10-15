# rieMiner

![rieMiner Logo](https://riecoin.xyz/Logos/Riecoin128.png)

rieMiner is a Riecoin miner supporting both solo and pooled mining, and can also be run standalone for prime constellation record attempts. Find the latest binaries [here](https://riecoin.xyz/rieMiner) for Linux and Windows.

This README is intended for advanced users and will mainly only describe the different configuration options and give information for developers like how to compile or contribute. For more practical information on how to use rieMiner like configuration file templates and mining or record attempts guides, visit the [rieMiner's page](https://riecoin.xyz/rieMiner). **Before asking for any help or reporting an issue, ensure that you followed the instructions correctly, and try first to solve the issues by yourself**.

Happy Mining or Good Luck on finding a new record!

## Recommended Requirements

* Windows 11 x64 (latest version) or Debian 12 x64
* AMD Zen 5 Processor (Ryzen 9000) or better, with efficient AVX512 implementation and 8 cores or more
* 16 GiB of RAM or more

We only provide binaries for Windows x64 and Linux x64 + Arm64. In other situations, you are on your own, and must have access to an appropriate build environment and compile yourself the code, as well as find out yourself which modifications might be required for an older version.

Memory usage can be reduced using the PrimeTableLimit option.

## Compile this program

rieMiner should be compilable on recent Linuxes. Building on another system is not supported, and Debian 12 Amd64 is our reference OS. It should be used for cross compiling in order to generate binaries for other systems, that can be distributed. If you do not have Debian 12, you can install it in a virtual machine, as Dual Boot, or in a spare machine.

### Native building

Here is how to build rieMiner to run it directly on the same computer. Libraries are dynamically linked and G++ optimizes the build for your CPU, though this does actually not affect performance much.

You will need g++, as, m4, make, and the following dependencies:

* [GMP](https://gmplib.org/)
* [cURL](https://curl.haxx.se/)
* [NLohmann Json](https://json.nlohmann.me/)

On a recent enough Debian or Ubuntu, you can easily install these if needed by doing as root:

```bash
apt install g++ make m4 git libgmp-dev libssl-dev libcurl4-openssl-dev nlohmann-json3-dev
```
And on an Arch based distribution:

```bash
pacman -S gcc make m4 git gmp curl nlohmann-json
```

For Apple Silicon:

```bash
brew install gcc make m4 git gmp curl nlohmann-json
```

Then, download the source files, go/`cd` to the directory:

```bash
git clone https://github.com/RiecoinTeam/rieMiner.git
cd rieMiner
```

Finally, do a simple make:

```bash
make
```

For Apple Silicon:
```bash
make apple_silicon
```

For other Linux, executing equivalent commands (using `pacman` instead of `apt`,...) should work.

### Static/Cross Compiling

To generate binaries that can be distributed, cross compiling is done on Debian 12 Amd64. This statically links the libraries and the compiler does not optimize for a particular processor. Here is how you can do it yourself.

Firstly, make sure that you are running Debian 12 x64. You may need to install some basic tools,

```bash
apt install make m4 git wget
```

You will also need appropriate compilers depending on the target system, use this table to find out what to install.

| Target        | Package to install via Apt    |
|---------------|-------------------------------|
| Windows x64   | `g++-mingw-w64-x86-64-posix`  |
| Linux x64     | none                          |
| Linux Arm64   | `g++-aarch64-linux-gnu`       |
| Android Arm64 | not in repository, see below  |

Now, get the rieMiner's source code.

```bash
git clone https://github.com/RiecoinTeam/rieMiner.git
cd rieMiner
```

#### Get Dependencies

Retrieve the dependencies' source codes by running the `GetDeps.sh` script,

```bash
sh GetDeps.sh
```

A folder named `rieMinerDeps` must have appeared.

#### Build Dependencies for Windows x64, Linux x64, or Linux Arm64

Now, use the `BuildDeps.sh` script to build the dependencies. This takes an argument for the target operating system, `Win64` or `Deb64`. For example,

```bash
sh BuildDeps.sh Win64
```

In this example, the `incsWin64` and `libsWin64` folders must have appeared.

Finally, build rieMiner itself.

```bash
cd ..
make Win64
```

or use `Win64AVX2` for AVX2 support.

For Linux x64, do the same steps, but with `Deb64` or `Deb64AVX2` instead of `Win64`. Note that GLibC is still linked dynamically for Linux builds so rieMiner may fail to run on very old Linux distributions.

For Linux Arm64, use `Arm64` instead of `Win64`.

If you are going to compile for several systems, doing `make clean`s in the rieMiner's directory will be very useful between builds (it will not delete rieMiner binaries). For example, once you have built the dependencies for `Win64`, `Deb64`, and `Arm64`, you can do

```bash
make clean ; make Win64
make clean ; make Win64AVX2
make clean ; make Deb64
make clean ; make Deb64AVX2
make clean ; make Arm64
```

Which yields the five expected binaries.

#### Build Dependencies for Android Arm64

Note that this will not produce an Apk file, only a binary that can be launched in something like Termux. There is also no built-in temperature control and you are responsible that the heat does not damage your device.

The compiler is not included in the Debian's repositories and you will need to install it manually: get the Android NDK Tools from [here](https://developer.android.com/ndk/downloads).

You should now have an `android-ndk-r25` folder or similar somewhere, remember its path.

You must now choose your target Android API Level. Each level correspond to a minimum Android version with which an application is compatible, for example API Level 30 corresponds to Android 11. A list can be found [here](https://developer.android.com/studio/releases/platforms). By default, it is set to 26.

Replace accordingly the `export ANDROIDAPI` and `export NDK` lines in the `BuildDeps.sh` file. Then, build the dependencies.

```bash
sh BuildDeps.sh And64
```

The `incsAnd64` and `libsAnd64` folders must have appeared next to the dependencies folder. You must now adapt the `And64: CXX =` line in the rieMiner's Makefile (outside the dependencies folder) before building rieMiner itself.

```bash
cd ..
make And64
```

## Configure this program

rieMiner uses a text configuration file, by default a "rieMiner.conf" file next to the executable. It is also possible to use custom paths, examples:

```bash
./rieMiner config/example.txt
./rieMiner "config 2.conf"
./rieMiner /home/user/rieMiner/rieMiner.conf
```

For other Modes or more options, you need to know how to write a configuration file yourself. The rieMiner.conf syntax is very simple: each option is set by a line like

```
Option = Value
```

It is case sensitive. A line starting with `#` will be ignored, as well as invalid ones. Spaces or tabs just before or after `=` are also trimmed. If an option is missing, the default value(s) will be used. If there are duplicate lines for the same option, the last one will be used.

Alternatively, command line options can be used like

```bash
./rieMiner config.conf Option1=Value1 "Option2 = Value2" Option3=WeirdValue\!\!
```

A configuration file path must always be provided. If the file exists, its options will be parsed first, then the command line ones, so the latter will override the common ones from the file. Else, it is just ignored, so just put a dummy value if you want to configure only by command line. The syntax of a command line option is the same as a line of the configuration file. You are responsible to correctly take care of special characters if needed.

### Modes

rieMiner proposes the following Modes depending on what you want to do. Use the `Mode` option to choose one of them (by default, `Benchmark`), below are the values to use.

* `Solo`: solo mining via GetBlockTemplate;
* `Pool`: pooled mining using Stratum;
* `Benchmark`: test performance with a simulated and deterministic network (use this to compare different settings or share your benchmark results);
* `Search`: pure prime constellation search (useful for record attempts).

### Solo and Pooled Mining options

* `Host`: URL of the Riecoin server. Default: 127.0.0.1 (your computer);
* `Port`: port of the Riecoin server (same as rpcport in riecoin.conf for solo mining). Default: 28332 (default RPC port for Riecoin Core);
* `Proxy`: URL of a Proxy (can be suffixed by the port, example `socks5://example.com:1080` - see [Curl Documentation](https://curl.se/libcurl/c/CURLOPT_PROXY.html)), optional. Default: none;
* `Username`: username used to connect to the server (same as rpcuser in riecoin.conf for solo mining). Default: empty;
* `Password`: password used to connect to the server (same as rpcpassword in riecoin.conf for solo mining). Default: empty;
* `Cookie`: for solo mining, cookie file containing the credentials, overriding `Username` and `Password`. Default: empty/none;
* `PayoutAddress`: payout address for solo mining. You can use Bech32 "ric1" addresses (only lowercase). Default: a donation address;
* `Rules`: for solo mining, add consensus rules in the GetBlockTemplate RPC call, each separated by a comma. You should not touch this unless a major Riecoin upgrade is upcoming and it is said to use this option. Default: none.

### Benchmark and Search Modes options

* `Difficulty`: for Benchmark and Search Modes, sets the difficulty (which is the number of binary digits, it must be at least 128). It can take decimal values and the numbers will be around 2^Difficulty. Default: 1024;
* `TupleLengthMin`: for Search Mode, the base prime of tuples of at least this length will be shown. 0 for the length of the constellation pattern - 1 (minimum 1). Default: 0;
* `BenchmarkBlockInterval`: for Benchmark Mode, sets the time between blocks in s. <= 0 for no block. Default: 150;
* `BenchmarkTimeLimit`: for Benchmark Mode, sets the testing duration limit in s. <= 0 for no time limit. Default: 86400;
* `BenchmarkPrimeCountLimit`: for Benchmark Mode, stops testing after finding this number of 1-tuples. 0 for no limit. Default: 1000000;
* `TuplesFile`: for Search Mode, write tuples of at least length TupleLengthMin to the given file. Default: Tuples.txt.

### More options

* `Threads`: number of threads used for mining, 0 to autodetect. Default: 0;
* `PrimeTableLimit`: the prime table used for mining will contain primes up to the given number. Set to 0 to automatically calculate according to the current Difficulty. You can try a larger limit as this will reduce the ratio between the n-tuple and (n + 1)-tuple counts (but also the candidates/s rate). Reduce if you want to lower memory usage. Default: 0;
* `SieveBits`: the size of the primorial factors table for the sieve is 2^SieveBits bits. 25 seems to be an optimal value, or 24 if there are many SieveWorkers. Though, if you have less than 8 MiB of L3 cache, you can try to decrement this value. Default: 25 if SieveWorkers <= 4, 24 otherwise;
* `SieveIterations`: how many times the primorial factors table is reused for sieving. Increasing will decrease the frequency of new jobs, so less time would be "lost" in sieving, but this will also increase the memory usage. It is not clear however how this actually plays performance wise, 16 seems to be a good value. Default: 16;
* `SieveWorkers`: the number of threads to use for sieving. Increasing it may solve some CPU underuse problems, but will use more memory. 0 for choosing automatically. Default: 0;
* `DifficultyOffset`: while Mining, the raw Difficulty Offset as per the Improved PoW Version 1 Implementation (integer between 0 and 2047). Useful for record attempts while mining rather than via the Search Mode. Default: 0;
* `RestartDifficultyFactor`: if the Difficulty changes by the given factor, the miner will restart. Useful to let it retune some parameters once a while to optimize for lower or higher Difficulties as it varies. This value must be at least 1 and the closer it is to 1 and the more often there will be restarts. Default: 1.03;
* `ConstellationPattern`: which sort of constellations to look for, as offsets separated by commas. Note that they are not cumulative, so '0, 2, 4, 2, 4, 6, 2' corresponds to n + (0, 2, 6, 8, 12, 18, 20). If empty (or not accepted by the server), a valid pattern will be chosen (0, 2, 4, 2, 4, 6, 2 in Search and Benchmark Modes). Default: empty;
* `PrimorialOffsets`: list of offsets from a primorial multiple to use for the sieve process, separated by commas. If empty, a default one will be chosen if possible (see main.hpp source file), otherwise rieMiner will not start (if the chosen constellation pattern is not in main.hpp). Default: empty;
* `RefreshInterval`: refresh rate of the stats in seconds. <= 0 to disable them and only notify when a long enough tuple or share is found, or when the network finds a block. Default: 30;
* `GeneratePrimeTableFileUpTo`: if > 1, generates the table of primes up to the given limit and saves it to a `PrimeTable64.bin` file, which will be reused instead of recomputing the table at every miner initialization. This does not affect mining, but is useful if restarting rieMiner often with large Prime Table Limits, notably for debugging or benchmarks. However, the file will take a few GB of disk space for large limits and you should have a fast SSD. Default: 0;
* `LogDebug`: if you don't want to log the console outputs and debugging info in debug files, set this to anything other than `Yes`. Default : enabled;
* `RawOutput`: if you need to disable the coloring in the outputs, set this to `Yes`. Default : disabled;
* `APIPort`: sets the port to use for the rieMiner's API server. 0 to disable the API. Default : 0;
* `KeepRunning`: if you need to continue running the miner even if you find a tuple in Search mode, set this to `Yes` Default : disabled.

## Interface

For now, rieMiner only works in a console. First, it will summarize the settings and print some information about the miner initialization, and you should look there to ensure that your settings were taken in account.

During mining, rieMiner will regularly print some statistics (use the `RefreshInterval` parameter to change the frequency). They consist of the candidates per second speed `c/s` and the 0 to 1-tuples/s ratio `r`. The estimate of the average time to find a block (for pooled mining, the earnings in RIC/day) is also shown. The number of tuples found since the start of mining is also shown (for pooled mining, the numbers of valid and total shares).

rieMiner will also notify if it found a block or a share, and if the network found a new block. If it finds a block or a share, it will tell if the submission was accepted (solo mining only) or not by the server.

In Benchmark and Search Modes, the behavior is essentially the same as Solo mining. In mining Modes, the statistics are based on the tuples found during the latest five blocks, including the current one, while in the other Modes, everything since the beginning is taken in account.

### Troubleshooting

rieMiner writes in `rieMiner_debug_YYYY-MM-DD_HHMMSS.log` files (named according to the start time, with possible suffixes if several instances are started at the same second), which contains more detailed outputs that can be useful when debugging rieMiner or asking for help.

### API

A basic API server is implemented in rieMiner. Currently, it only provides simple statistics and the version, and it will be improved and completed in the future. Use the `APIPort` option to choose the port. Methods:

* `getstats`/`getstatsjson`: `getstats` returns the following stats in a simple format (one line per entry) for easy parsing: whether the miner is running (true of false), since how much time in s, the candidates/s, the ratio, the blocks/day, the mining power, the number of shares found and the number of rejected shares (both always 0 if not pooled mining). `getstatsjson` formats these in JSON;
* `getminerinfo`/`getminerinfojson`: `getminerinfo` returns the miner's name and the version (one line per entry). `getminerinfojson` formats these in JSON.

If you have `netcat`, you can do for example

```bash
echo -n getstatsjson | nc localhost 2001
```

to get

```bash
{"running": true, "uptime": 1547.53, "cps": 15286.2, "r": 16.5296, "bpd": 3.91715, "miningpower": 0.433537, "shares": 169, "sharesrejected": 0}
```

## Developers and license

* [Pttn](https://github.com/Pttn), author and maintainer, contact: dev at Pttn dot me

This work is released under the GPLv3 license.

### Notable contributors

* [Michael Bell](https://github.com/MichaelBell/): assembly optimizations, improvements of work management between threads, and some more.

[More Contributors](https://github.com/Pttn/rieMiner/graphs/contributors)

## Contributing

Feel free to do a pull request or open an issue, and I will review it. I am open for adding new features, but I also wish to keep this project minimalist. Any useful contribution will be welcomed.

By contributing to rieMiner, you accept to place your code in the GPL license.

Donations to the Riecoin Project are welcome:

* Riecoin: ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn
* Bitcoin: bc1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsewte3f

### Testing

Code for a testing server is provided. It acts like a mining pool, and tests the rieMiner's behavior in various network situations like whether it restarts properly when the Difficulty increases a lot or if it reconnects when there are disconnects. It was only tested on Debian 11. The server can be built and run with

```bash
make testServer
./rieMinerTestServer
```

Then, launch rieMiner using the `Pool` Mode and Port `3004`. Watch whether strange things happen, if there are crashes or deadlocks, test with several machines and different rieMiner parameters, run several loops, also do not hesitate to changes some parameters in the code...

### Quick contributor's checklist

* Your code must compile and work on recent Debian based distributions, and Windows using MSYS;
* If modifying the miner, you must ensure that your changes do not cause any performance loss. You have to do proper and long enough before/after benchmarks;
* Document well non trivial contributions to the miner so other and future developers can understand easily and quickly the code;
* rieMiner must work for any realistic setting, you should make tests with the Test Server;
* Ensure that your changes did not break anything, even if it compiles. Examples (if applicable):
  * There should never be random (or not) segmentation faults or any other bug, try to do actual mining with Gdb, debugging symbols and Debug Mode enabled during hours or even days to catch possible bugs;
  * Ensure that valid work is produced (pools and Riecoin Core must not reject submissions);
  * Mining must stop completely while disconnected and restart properly when connection is established again.
* Follow the style of the rest of the code (curly braces position, camelCase variable names, tabs and not spaces, spaces around + and - but not around * and /,...).
  * Avoid using old C style and prefer modern C++ code;
  * Prefer longer and explicit variable names (except for loops indexes where single letter variables should be used in most cases).

## Resources and other links

* [Riecoin website](https://riecoin.xyz/)
  * [rieMiner's page](https://riecoin.xyz/rieMiner/)
  * [Explanation of the miner algorithm](https://riecoin.xyz/Guides/MiningAlgorithm/), you can also learn the theoretics behind some options
* [Stella](https://github.com/Pttn/Stella), possible reimplementation in Rust
* [Bitcoin Wiki - Getblocktemplate](https://en.bitcoin.it/wiki/Getblocktemplate)
* [BIP141](https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki) (Segwit)
* [Bitcoin Wiki - Stratum](https://en.bitcoin.it/wiki/Stratum_mining_protocol)
