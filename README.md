# CEagle

CEagle installs programs on your TI-84 Plus CE straight from [ceagle.cc](https://ceagle.cc). Browse and search projects, install them with everything they need, keep them up to date and remove them again, all on the calculator.

## What you need

| What | Why |
| --- | --- |
| A TI-84 Plus CE | |
| A USB Ethernet adapter on an OTG cable, or the lwIP-CE Wi-Fi bridge | The network |
| [lwIP-CE](https://github.com/cagscalclabs/lwip-ce) 3.0 | The network stack. Follow its [getting started guide](https://cagscalclabs.github.io/lwip-ce/getting-started.html) and run its installer, `prgmINSTALL`, once |
| The [CE C libraries](https://tiny.cc/clibs) (`clibs.8xg`) | CEagle and lwIP-CE both need them |

## Install

Send `CEAGLE.8xp` to your calculator, plug in the network adapter and run `prgmCEAGLE` from the home screen. CEagle gets a network address on its own; the first start can take a few seconds.

To update CEagle itself, install the project `ceagle` from inside CEagle, then quit and start it again.

## Using it

The five labels at the bottom of the screen belong to the five keys under the screen, y= to graph.

| Screen | Keys |
| --- | --- |
| Browse | up and down to choose, enter to open, y= to search, window to switch between All, BASIC, ASM/C, Libraries and AppVars, left and right for more pages, zoom (labelled Updates) for your installed projects, trace for settings, clear to quit |
| Project | enter to install or update, del to uninstall, clear to go back |
| Review | up and down to look through the projects an install needs, enter to install, clear to cancel |
| Installed | enter to update the selected project, del to uninstall it, clear to go back |
| Settings | up and down to choose, enter to change the server address or to turn pre-releases on and off |

In a text field, alpha switches between letters and digits, del erases, enter confirms and clear empties the field or cancels. Clear also stops a download at any moment.

## What CEagle does for you

- Before an install that needs other projects or can do something risky, it shows a review: every project it will install or update, how much it downloads, and what the program can do, like "Deletes variables" or "Writes to flash memory". Nothing happens until you press enter.
- It installs every library and file a project needs, and checks that there is enough free RAM and archive before it writes anything.
- Every file is checked against its size and SHA-256 while it downloads. A broken or cut off download is deleted, and nothing else changes.
- It never overwrites a variable that another installed project owns or that you made yourself.
- It keeps a library as long as an installed program still needs it.
- If a project you installed is taken down from CEagle, it tells you and offers to delete it.

Programs, appvars, strings and matrices go to the archive when the author asks for it. Lists, pictures and images always stay in RAM.

CEagle keeps its list of installed projects in the appvar `CEAGLEDB` and its settings in `CEAGLECF`, both archived, so a RAM reset keeps them.

## Building

You need the [CE C toolchain](https://github.com/CE-Programming/toolchain) with lwIP-CE 3.0 installed into it, so that `lwip.h` and its library are found.

```sh
make
```

This builds `bin/CEAGLE.8xp`. The host unit tests need `clang` and OpenSSL's `libcrypto`:

```sh
make -C tests/host
```

## License

CEagle is free software under the GNU General Public License, version 3. See `LICENSE`.
