# RV640

Very simple RISC-V emulator that can (barely) run Linux.


## Supported platforms

* Linux
* DOS 6.22
  * IBM-PC XT class machine (8088 or better)
  * 640KB of memory
  * Sufficiently large disk to store guest RAM image (32MB minimum)
  * CGA or better video adapter

## Building

To build the emulator and Linux image in Docker environment:
```bash
./build.sh
```
Build artifacts will be created in the `dist` directory.

See `build.Dockerfile` for more details

## Running

In the guest OS - login as `root` with no password.

### Linux
```bash
cd linux
./RV640 --dtb rv640.dtb --kernel ../Image
```

### DOS
```bash
cd dos
RV640 --swap ram.swp --dtb rv640.dtb --kernel ../Image
```
Swap file contains the entire RAM of the guest VM. It will be created/resized automatically. The faster the underlying storage - the better.

RV64 VM executes 200-250 IPS on average on 8086 @ 4.77MHz. Be prepared to wait for several days for a login prompt.
The experience is not very interactive. There will be a (extremely) delayed feedback while typing.
Emulator has a 256 byte input buffer, so just blindly type your command and return in several hours...

---

## Acknowledgements

The idea came after reading this article:
[Linux on an 8-bit micro?
](https://dmitry.gr/?r=05.Projects&proj=07.%20Linux%20on%208bit)
