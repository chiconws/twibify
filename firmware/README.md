# Firmware

**English** | [Português](README.pt-BR.md)

| File | Size | SHA-256 |
|---|---|---|
| `twibify-v1.0.bin` | 6,251,604 B | `866b3c9bb83f849704cde0800debabcbd096f1062f790e2f402c3c0bbcf02a2f` |

Format: **uImage (64-byte header) + kernel (stock 1.1.11) + squashfs rootfs + trailer**,
built by `tools/mkimage_twibi.py` with the rootfs checksum recomputed.

## Install

The bootloader takes the image **without the 64-byte header**:

```sh
python3 -c "d=open('twibify-v1.0.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"

# device in disaster mode, PC in 192.168.1.0/24:
python3 ../tools/tftp_put.py 192.168.1.6 payload.bin --name payload.bin
```

Full procedure: [`../docs/install.md`](../docs/install.md).

## Provenance

- The **kernel** and the **base rootfs** are Intelbras/Tenda property; this image is a
  derivative distributed for **study and personal use on the device itself**.
- No personal data is included: it is a firmware image, not a flash dump (no MACs, no
  Wi-Fi keys, no config).
- The configuration partition (CFM) is **not** part of the image, so your network/password
  settings stay as they are when you flash it.
