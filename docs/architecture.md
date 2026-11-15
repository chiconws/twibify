# Architecture

**English** | [Português](architecture.pt-BR.md)

Twibify is the **stock 1.1.11 rootfs** with a small set of changes; the kernel, the
bootloader and the partitions are the official ones. The image is built by unpacking the
official squashfs (`unsquashfs`), editing the tree, repacking (`mksquashfs`, xz, 128 KB)
and rebuilding the uImage with the rootfs checksum (`tools/mkimage_twibi.py`).

## What changes in the tree

| Path | Change |
|---|---|
| `usr/sbin/telnetd` | **removed** (symlink) |
| `bin/cwmpd` | **removed** (TR-069) |
| `bin/ucloud` | **kept** (cloud/app) |
| `usr/sbin/dropbear`, `usr/bin/dropbearkey` | added (static) |
| `usr/sbin/iot-clients` | added (MAC group + iptables) |
| `usr/sbin/iotweb` | added (HTTP backend on :8080) |
| `webroot_ro/iot.html` | added (the page) |
| `webroot_ro/pages/tr069.js` | replaced by the IoT chunk (iframe to `/iot.html?t=`) |
| `webroot_ro/pages/app.js` | menu/path `tr069` → `iot` |
| `webroot_ro/index.html`, `login.html` | footer (branding + live status) |
| `webroot_ro/default.cfg` | `cwmpd.en=0`, `sys.itbupgrade.enable=0` (URL empty), `sys.schedulereboot.enable=0`, `sched.reboot.enable=0` |
| `etc_ro/passwd`, `etc_ro/shadow` | root hash `$1$` of `chiconws`, home `/var/root` |
| `etc_ro/issue`, `chiconws_version`, `marker_custom.txt` | branding/version markers |
| `etc_ro/init.d/rcS` | the block below |

## Boot flow (rcS additions)

```
monitor &                        # stock daemons (cfm, netctrl, httpd, ucloud, ...)
|
+-- (sleep 15) cfm Set cwmpd.en 0 ... ucloud.en 1   # CMF survives flash
+-- dropbear + host key
+-- (sleep 20) token -> /webroot/iot.token ; iotweb &
+-- (sleep 30) apply_ssh ; while true: sleep 30; apply_ssh; iot-clients apply
+-- (loop 10s) iotweb watchdog
```

### The `cfmd` race

On this firmware, **dropbear authenticates against `/etc/passwd`** (not `/etc/shadow`).
`cfmd`/`netctrl` rewrite `/etc/passwd` **after** `rcS`, so a password written too early is
lost. Symptoms: the console loops `sulogin: no pass` and SSH rejects everything.

Twibify therefore applies the hash **after** the config daemons and keeps re-applying it
every 30 s. The source of truth is `cfm ssh.pwd`; if it is empty, the built-in `chiconws`
hash is used. The hash is written to **both** `/etc/passwd` and `/etc/shadow`.

## UI integration

The stock web UI is a Vue SPA. The TR-069 route (`chunk 16`, `pages/tr069.js`) was replaced
by a minimal chunk that renders an **iframe** to `/iot.html`, and the menu entry was
renamed to **IoT** with path `/iot`. The iframe URL carries `?t=Date.now()` so the page is
never served from cache.

The page is only reachable with a valid web session (the httpd redirects anonymous HTML
requests to `login.html`).

## Backend security model

- `iotweb` listens on `:8080` and requires `Authorization: Bearer <token>`.
- The token is random **per boot**, written by `rcS` to `/webroot/iot.token` (mode 600).
  The httpd serves that file only to an authenticated session, so the page can read it
  while an anonymous client cannot — this also covers CSRF (an external page cannot read
  the token).
- The backend runs as root (it needs `iptables`, `/etc/passwd` and `cfm`), and does not
  fork for the status endpoint (it reads `/proc` directly).

## Limits and related research

Per-client L2 isolation is not possible on this firmware: the switch driver exposes
VLANs, but the LAN port cannot join a second VLAN, there is no MAC-based VLAN and the
3.10 kernel has no bridge VLAN filtering/ebtables. The full investigation is in
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re)
(`docs/vlan.md`).

The mesh stack (`mpp`, `nos.ko`, `/proc/mesh/*`) is untouched. A stock node is expected to
pair with a Twibify master (same 1.1.11 base), but that was not tested yet.
