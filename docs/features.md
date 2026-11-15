# Features

**English** | [Português](features.pt-BR.md)

## SSH (dropbear)

- starts at boot from `rcS`; host key generated on first boot;
- **off by default**: the switch on the page enables it, and enabling requires a password
  (min 8 chars). The UART console keeps `chiconws` for recovery;
- change it on the IoT page (minimum 8 chars) — the hash is stored in `cfm ssh.pwd` and
  re-applied after `cfmd`/`netctrl` rewrite `/etc/passwd`;
- paste one public key on the page; it goes to `cfm ssh.key` and is written to
  `/var/root/.ssh/authorized_keys` at boot (root's home is `/var/root`, since `/` is
  read-only).

The command line works as usual:

```sh
ssh root@<ip>                 # password
ssh -i ~/.ssh/key root@<ip>   # key
```

## IoT group by MAC

There is **no IoT IP range**. The group is a list of MACs:

```sh
iot-clients add aa:bb:cc:dd:ee:ff
iot-clients del aa:bb:cc:dd:ee:ff
iot-clients list
iot-clients status
iot-clients clients          # ARP + leases
iot-internet on|off          # whole group
```

How it works: an `iptables` chain `CH_iot` is hooked in `FORWARD` (`-i br0 -j CH_iot`);
each listed MAC gets a `DROP` rule. A watchdog re-applies the chain every 30 s because
`netctrl` rewrites the firewall on network events. The list and the on/off flag live in
the CMF (`iot.clients`, `iot.internet`), so they survive reboots.

Because it matches the **source MAC**, wired clients behind an unmanaged switch and Wi-Fi
clients are treated the same. It is **L3 only**: it blocks internet, not traffic between
two clients on the same LAN.

## Web UI page ("IoT")

The TR-069 menu slot is now **IoT** (same look as the other pages: green buttons, dark
theme). The route is `#/iot`.

- **Clients**: IP(s) + MAC + group (IoT/LAN) + button to toggle. Entries are deduplicated
  by MAC (the ARP table can hold several IPs for the same device).
- **SSH password**: set a new root password.
- **Public key**: paste / remove a key.
- **Status strip**: RAM used/total (%), thermal 2.4G/5G (relative ADC, not °C) and uptime.
- **Footer** (in the whole UI and login): Twibify version, repo link and a live status
  line updated every second.

## System behaviour

| Key | Value | Meaning |
|---|---|---|
| `cwmpd.en` | `0` | TR-069 disabled (binary also removed) |
| `ucloud.en` | `1` | cloud **kept** — the app still manages the mesh |
| `sys.itbupgrade.enable` | `0` | OTA disabled (URL emptied) |
| `sys.schedulereboot.enable` | `0` | daily reboot off |
| `sched.reboot.enable` | `0` | the second scheduler off |
| `adv.upnp.en` | whatever you set | not changed by the firmware |

The values are forced by `rcS` through `cfm` because the CMF survives a flash and would
otherwise keep the old ones. Telnet was removed (`telnetd` link), so `/goform/telnet`
fails.

## HTTP API (port 8080)

The page talks to a small backend on `:8080`, authenticated with a **per-boot token**
written to `/webroot/iot.token` (only served to a logged-in web session):

| Endpoint | Content |
|---|---|
| `GET /api/status` | version, key state, RAM, thermal, uptime |
| `GET /api/clients` | ARP clients + IoT flag |
| `POST /api/iot` | `{"mac":"..","iot":true/false}` |
| `GET /api/ssh` | `{"enabled":..,"key":..}` |
| `POST /api/ssh/enable` | `{"password":".."}` — sets the password, starts dropbear |
| `POST /api/ssh/disable` | stops dropbear (`ssh.enabled=0`) |
| `POST /api/ssh/password` | `{"password":".."}` |
| `POST /api/ssh/key` | `{"key":"ssh-ed25519 ..."}` |
| `DELETE /api/ssh/key` | remove the key |

```sh
TOKEN=$(curl -s -b cookie http://<ip>/iot.token)
curl -H "Authorization: Bearer $TOKEN" http://<ip>:8080/api/status
```
