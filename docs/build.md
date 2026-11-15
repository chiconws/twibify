# Building

**English** | [Português](build.pt-BR.md)

The image is the official 1.1.11 rootfs with the changes listed in
[`architecture.md`](architecture.md). Everything needed to rebuild it (except the stock
image itself) is in this repository.

## Requirements

- Linux host with `unsquashfs`/`mksquashfs`;
- Python 3 + [`tftpy`](https://pypi.org/project/tftpy/) (`pip install tftpy`);
- `gcc-mipsel-linux-gnu` for the backend (`apt install gcc-mipsel-linux-gnu` on
  Debian/Ubuntu);
- the static dropbear/dropbearkey binaries (mipsel, musl) — the same ones used by the
  previous project; or build them yourself;
- the official `twibi_fast_1.1.11.bin` from
  [`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re)
  (also used as the uImage header template).

## 1. Extract kernel and rootfs

The official `.bin` is `uImage header (64 B) + kernel (lzma) + squashfs + trailer`. The RE
repo has `tools/unpack_kernel.py` for the kernel; the rootfs starts at `0x40 + kernel
size` (0x1ED412 for 1.1.11).

```sh
python3 ../intelbras-twibi-fast-re/tools/unpack_kernel.py twibi_fast_1.1.11.bin
# kernel.lzma + rootfs.squashfs
unsquashfs -d rootfs rootfs.squashfs
```

## 2. Apply the changes

| Item | Source in this repo |
|---|---|
| `usr/sbin/dropbear`, `usr/bin/dropbearkey` | static binaries (not included) |
| `usr/sbin/iotweb` | built from `src/iotweb.c` + `src/md5crypt.c` |
| `usr/sbin/iot-clients` | `src/iot-clients` (chmod 755) |
| `webroot_ro/iot.html` | `src/iot.html` |
| `webroot_ro/pages/tr069.js` | `src/tr069-chunk.js` |
| `etc_ro/init.d/rcS` | append `src/rcS.twibify` |
| `webroot_ro/default.cfg` | `cwmpd.en=0`, `sys.itbupgrade.enable=0` + empty URL, `sys.schedulereboot.enable=0`, `sched.reboot.enable=0` |
| `webroot_ro/pages/app.js` | `tr069` menu/route → `iot` |
| `webroot_ro/index.html`, `login.html` | footer with version/link/status |
| `etc_ro/passwd`, `etc_ro/shadow` | `root` hash `$1$` of `chiconws`, home `/var/root` |
| `etc_ro/issue`, `etc_ro/chiconws_version` | banner and version |

Generate the root password hash with the same code the backend uses:

```sh
gcc -O2 -DMD5CRYPT_MAIN -o md5crypt src/md5crypt.c
./md5crypt chiconws chiconws        # -> $1$chiconws$...
```

Then put `root:<hash>:0:0:root:/var/root:/bin/sh` in `etc_ro/passwd` and
`root:<hash>:14319::::::` in `etc_ro/shadow`. Remove `usr/sbin/telnetd` and `bin/cwmpd`.

## 3. Build the backend

```sh
mipsel-linux-gnu-gcc -static -Os -s -o rootfs/usr/sbin/iotweb src/iotweb.c
```

(`iotweb.c` includes `md5crypt.c`, so keep both in the same directory.)

## 4. Repack and build the image

```sh
mksquashfs rootfs rootfs.squashfs -noappend -comp xz -b 128K
python3 tools/mkimage_twibi.py \
  --kernel kernel.lzma --rootfs rootfs.squashfs \
  --out twibify.bin --template twibi_fast_1.1.11.bin
```

The tool recomputes the rootfs checksum the bootloader validates. Verify with the sum
being zero, then flash:

```sh
python3 -c "d=open('twibify.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"
python3 tools/tftp_put.py 192.168.1.6 payload.bin
```

## Notes

- Keep the **kernel** from the official image — only the rootfs changes.
- The image must stay below the kernel+rootfs region (7,569,408 B at `0x30000–0x7C0000`);
  Twibify 1.0 uses 6,796,308 B.
- If the squashfs grows, drop unused packages from the stock rootfs before repacking.
