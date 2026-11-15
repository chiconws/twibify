# Twibify

**English** | [Português](README.pt-BR.md)

A minimal, reversible modification of the **Intelbras Twibi Fast** firmware (Tenda OEM,
Realtek RTL8197F): **SSH**, a per-MAC **IoT group**, and a management page **inside the
original web UI** — while keeping the cloud, so the app still manages the mesh.

Built on the stock **1.1.11** rootfs, with the stock kernel and partitions untouched.

> ⚠️ Personal project on my own device. Not affiliated with Intelbras or Tenda. Flashing
> voids the warranty and can brick the unit; the bootloader's disaster mode is the safety
> net. Use at your own risk.

---

## Why

The stock firmware is fine — except it exposes **telnet**, keeps **TR-069** and the cloud
enabled, reboots every day at 03:00, and has no per-client internet control. Twibify
changes only that, and adds what was missing:

| Feature | Stock | Twibify |
|---|---|---|
| Remote access | telnet on demand | **SSH** (dropbear, password + public keys) |
| TR-069 (`cwmpd`) | enabled | **disabled** (binary removed) |
| Cloud (`ucloud`) | enabled | **kept** — the app still manages the mesh |
| OTA (`sys.itbupgrade`) | enabled | **disabled** (URL emptied) |
| Daily reboot 03:00 | enabled | **disabled** (both keys) |
| IoT | — | **per-MAC group**: internet on/off per client |
| UI | — | **IoT & SSH page** inside the original web UI (TR-069 slot) |
| Branding | Intelbras | Intelbras + "firmware paralelo" footer with version and repo |

## Features

### SSH

- static dropbear on port 22, started by `rcS`;
- **SSH is off by default**; a switch on the IoT page enables it, and enabling it requires
  setting a password (min 8 chars). The image keeps a console (UART) password, `chiconws`,
  for recovery;
- public keys: paste one on the page; it is stored in `cfm` and applied to
  `/var/root/.ssh/authorized_keys` at boot;
- password auth and key auth work together.

### IoT group by MAC

There is **no fixed IoT IP range** — that used to collide with mesh node addressing.
Membership is a list of **MACs**, so it works the same for wired clients (even behind a
switch) and Wi-Fi:

```sh
iot-clients add aa:bb:cc:dd:ee:ff
iot-clients del aa:bb:cc:dd:ee:ff
iot-clients list
iot-internet on|off      # the whole group
```

`iptables` chain `CH_iot` (hooked in `FORWARD` for `br0`) drops the WAN egress of each
listed MAC. A watchdog re-applies it every 30 s because `netctrl` rewrites the firewall.

### Web UI page

The TR-069 page slot now hosts **IoT & SSH** (same menu, same look: green buttons, dark
theme):

- client list (deduplicated by MAC) with a one-click "IoT" toggle;
- SSH password form and public-key field;
- status strip: RAM, 2.4G/5G thermal (relative ADC), uptime;
- the footer shows the firmware version + **live status** (updated every second).

## Install

The image is [`firmware/twibify-v1.0.bin`](firmware/twibify-v1.0.bin) (uImage format,
6,251,604 bytes). The bootloader takes the image **without the 64-byte header**, so strip
it and send via TFTP in disaster mode:

```sh
python3 -c "d=open('twibify-v1.0.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"

# device in disaster mode, PC in 192.168.1.0/24:
python3 tools/tftp_put.py 192.168.1.6 payload.bin --name payload.bin
```

To enter disaster mode: power off, hold **reset**, power on and keep it for ~15 s. The
bootloader prints `Entry disaster mode` and starts a TFTP server on `192.168.1.6`.

**Rollback**: the same procedure with the official `twibi_fast_1.1.11.bin` (see
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re)).
The bootloader is never touched.

Full steps: [`docs/install.md`](docs/install.md).

## Notes and limits

- **No L2 isolation per client**: the switch driver exposes VLANs (`/proc/net/vlan`) and
  `eth0.9` bridging works, but the LAN port cannot join a second VLAN, there is no
  MAC-based VLAN and the 3.10 kernel has no bridge filtering/ebtables. The IoT group is
  **L3** (internet blocked; LAN reachable).
- **Mesh with a modified master** was not tested with a stock node yet; the mesh stack is
  untouched and the base is the same 1.1.11, so it is expected to pair.
- **The stock web upgrade is a stub**: `POST /goform/upgrade` returns
  `{"errcode":"1005"}` even for the official image, so updates are done with disaster
  mode (or the cloud OTA, which installs the official firmware — keep it disabled).
- The **admin web password** is not changed by this firmware; it stays whatever the
  device has.

## Repository layout

```
docs/        install, features, architecture and build notes
firmware/    twibify-v1.0.bin + instructions
tools/       mkimage_twibi.py (checksum/image) and tftp_put.py (disaster mode)
images/      board photo
```

## Credits

Developed with **DeepSeek** — reverse engineering, firmware build, backend, UI and docs.
The reverse engineering notes live in
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re).

## License

MIT — see [`LICENSE`](LICENSE). The firmware image is a **derivative of Intelbras'
firmware** (same kernel and base rootfs); it is distributed for study and personal use of
the device itself.
