# Install and rollback

**English** | [Português](install.pt-BR.md)

Twibify is flashed through the bootloader's **disaster mode**, not through the web UI (the
stock local upgrade returns `1005` even for the official image — see the RE repo).

## What you need

- the Twibi Fast (any 1.1.x stock firmware; the kernel is kept);
- an Ethernet cable from the PC to a LAN port;
- TFTP client (`tools/tftp_put.py`, built on `tftpy`);
- optional: a 3.3 V FTDI adapter on the UART pads to watch the boot (115200 8N1).

## Before

The flash replaces **kernel + rootfs** only. The bootloader, the MAC/label areas, the
**CFM** (config) and the log partition are not touched — so your LAN IP, Wi-Fi and admin
password stay as they are. Keep a copy of `twibi_fast_1.1.11.bin` (official) for rollback.

## Flash

1. Power off the device.
2. Hold **reset**, power on **still holding**, keep it for ~15 s, release. The console
   prints `Entry disaster mode` and the bootloader serves TFTP on `192.168.1.6`.
3. Put the PC on `192.168.1.0/24` (e.g. `192.168.1.5/24`).
4. Strip the 64-byte header and send:

```sh
python3 -c "d=open('twibify-v1.0.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"
python3 tools/tftp_put.py 192.168.1.6 payload.bin --name payload.bin
```

The bootloader answers:

```
**TFTP Client Upload File Size = ... Success!
Linux kernel (root-fs) upgrade.
checksum Ok !
burn kernel-fs ...
squashfs upgrade. checksum Ok !
burn rootfs ...
```

It reboots by itself (~60–90 s until the web UI answers).

## First boot

- LAN keeps the address it had (the CFM survives). The login is the existing admin
  password.
- SSH: **off by default**. Open the **IoT** page, set a password and click **Ligar SSH**
  (the page is in Portuguese). The UART console still uses `chiconws`.
- The IoT page is in "Configurações avançadas → IoT".

## If it does not boot

The bootloader is intact — go back to disaster mode and flash the official image
(`twibi_fast_1.1.11.bin`, strip the header the same way). No UART is required for the
transfer; it only helps to see what happened.

## Rollback to stock

Same procedure with the official image from
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re).
A factory reset after that returns the default configuration.
