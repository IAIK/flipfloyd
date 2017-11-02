# Tools for "Another Flip in the Wall"

## Hammer tool

This Rowhammer tool tests whether your system is susceptible for one-location hammering bit flips.

See https://github.com/IAIK/rowhammerjs/tree/master/native for a double-sided hammering test (which is more likely to find bit flips on your system).

Also see our paper "Another Flip in the Wall of Rowhammer Defenses": https://arxiv.org/abs/1710.00551

### Build and Run

```
make
./rowhammer <gigabytes of memory to use>
```

The more gigabytes the better, but too much will bring your system out of memory.
60-90% of the available memory is a good range.

How long will it run? It might run days without bit flips. If it finds bit flips on your machine, please open an issue or send us a message, we'd like to see your results!

## Memory waylaying tool

These tools implement two parts of waylaying.

The ''check'' tool prints the physical address of the first page of the specified binary.

The ''relocate'' tool implements memory chasing (the non-stealthy variant of waylaying) to force relocation of a binary page.

### Build and Run

```
make
sudo ./check /usr/bin/sudo   # sudo to get the physical address
./relocate <gigabytes of system memory> /usr/bin/sudo
sudo ./check /usr/bin/sudo   # sudo to get the physical address
```

The second run of the check tool will show a new physical address for the same binary page.

## Demos

### One-Location Hammering Demo (Skylake i7 DDR4-2133)

![One-Location Hammering Demo on a Skylake i7 DDR4-2133](https://raw.githubusercontent.com/IAIK/flipfloyd/master/lab05.gif)

### One-Location Hammering Demo (Haswell i7 DDR3-1600)

![One-Location Hammering Demo on a Haswell i7 DDR3-1600](https://raw.githubusercontent.com/IAIK/flipfloyd/master/lab02.gif)

### Waylaying Demo (Skylake i5 12 GB RAM)

![Waylaying Demo on a Skylake i5 with 12 GB of RAM](https://raw.githubusercontent.com/IAIK/flipfloyd/master/waylaying.gif)

## Warnings

**Warning #1:** We are providing this code as-is.  You are responsible
for protecting yourself, your property and data, and others from any
risks caused by this code.  This code may cause unexpected and
undesirable behavior to occur on your machine.  This code may not
detect the vulnerability on your machine.

Be careful not to run this test on machines that contain important
data.  On machines that are susceptible to the rowhammer problem, this
test could cause bit flips that crash the machine, or worse, cause bit
flips in data that gets written back to disc.

**Warning #2:** If you find that a computer is susceptible to the
rowhammer problem, you may want to avoid using it as a multi-user
system.  Bit flips caused by row hammering breach the CPU's memory
protection.  On a machine that is susceptible to the rowhammer
problem, one process can corrupt pages used by other processes or by
the kernel.

