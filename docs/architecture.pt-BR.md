# Arquitetura

[English](architecture.md) | **Português**

O Twibify é o **rootfs oficial 1.1.11** com um conjunto pequeno de mudanças; o kernel, o
bootloader e as partições são os oficiais. A imagem é montada desempacotando o squashfs
oficial (`unsquashfs`), editando a árvore, reempacotando (`mksquashfs`, xz, 128 KB) e
reconstruindo a uImage com o checksum do rootfs (`tools/mkimage_twibi.py`).

## O que muda na árvore

| Caminho | Mudança |
|---|---|
| `usr/sbin/telnetd` | **removido** (symlink) |
| `bin/cwmpd` | **removido** (TR-069) |
| `bin/ucloud` | **mantido** (nuvem/app) |
| `usr/sbin/dropbear`, `usr/bin/dropbearkey` | adicionados (estáticos) |
| `usr/sbin/iot-clients` | adicionado (grupo MAC + iptables) |
| `usr/sbin/iotweb` | adicionado (backend HTTP na :8080) |
| `webroot_ro/iot.html` | adicionada (a página) |
| `webroot_ro/pages/tr069.js` | trocado pelo chunk IoT (iframe para `/iot.html?t=`) |
| `webroot_ro/pages/app.js` | menu/rota `tr069` → `iot` |
| `webroot_ro/index.html`, `login.html` | rodapé (branding + status ao vivo) |
| `webroot_ro/default.cfg` | `cwmpd.en=0`, `sys.itbupgrade.enable=0` (URL vazia), `sys.schedulereboot.enable=0`, `sched.reboot.enable=0` |
| `etc_ro/passwd`, `etc_ro/shadow` | hash `$1$` do root `chiconws`, home `/var/root` |
| `etc_ro/issue`, `chiconws_version`, `marker_custom.txt` | marcações de branding/versão |
| `etc_ro/init.d/rcS` | o bloco abaixo |

## Fluxo de boot (adições no rcS)

```
monitor &                        # daemons originais (cfm, netctrl, httpd, ucloud, ...)
|
+-- (sleep 15) cfm Set cwmpd.en 0 ... ucloud.en 1   # o CMF sobrevive ao flash
+-- host key; dropbear só se cfm ssh.enabled=1
+-- (sleep 20) token -> /webroot/iot.token ; iotweb &
+-- (sleep 30) apply_ssh ; while true: sleep 30; apply_ssh; iot-clients apply
+-- (loop 10s) watchdog do iotweb
```

### A corrida com o `cfmd`

Neste firmware o **dropbear autentica pelo `/etc/passwd`** (não pelo `/etc/shadow`). O
`cfmd`/`netctrl` reescreve o `/etc/passwd` **depois** do `rcS`, então uma senha escrita
cedo demais se perde. Sintomas: o console entra em loop `sulogin: no pass` e o SSH recusa
tudo.

Por isso o Twibify aplica o hash **depois** dos daemons de config e continua reaplicando a
cada 30 s. A fonte da verdade é o `cfm ssh.pwd`; se estiver vazio, usa o hash embutido
`chiconws`. O hash é escrito **no** `/etc/passwd` **e** no `/etc/shadow`.

## Integração com a UI

A webui original é uma SPA Vue. A rota do TR-069 (`chunk 16`, `pages/tr069.js`) foi
substituída por um chunk mínimo que renderiza um **iframe** para `/iot.html`, e a entrada
do menu virou **IoT** com caminho `/iot`. A URL do iframe leva `?t=Date.now()` para nunca
vir do cache.

A página só é alcançável com sessão válida na web (o httpd redireciona HTML anônimo para
`login.html`).

## Modelo de segurança do backend

- O `iotweb` escuta na `:8080` e exige `Authorization: Bearer <token>`.
- O token é aleatório **por boot**, escrito pelo `rcS` em `/webroot/iot.token` (modo 600).
  O httpd só entrega esse arquivo para sessão autenticada, então a página consegue lê-lo
  enquanto um cliente anônimo não — isso também cobre CSRF (uma página externa não
  consegue ler o token).
- O backend roda como root (precisa de `iptables`, `/etc/passwd` e `cfm`) e não faz fork no
  endpoint de status (lê o `/proc` direto).

## Limites e pesquisa relacionada

Isolamento L2 por cliente não é possível neste firmware: o driver do switch expõe VLANs,
mas a porta LAN não entra em uma segunda VLAN, não existe VLAN por MAC e o kernel 3.10 não
tem bridge VLAN filtering/ebtables. A investigação completa está em
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re)
(`docs/vlan.pt-BR.md`).

O stack de mesh (`mpp`, `nos.ko`, `/proc/mesh/*`) está intocado. Espera-se que um node
stock pareie com um mestre Twibify (mesma base 1.1.11), mas isso ainda não foi testado.
