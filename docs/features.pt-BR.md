# Recursos

[English](features.md) | **Português**

## SSH (dropbear)

- sobe no boot pelo `rcS`; a host key é gerada no primeiro boot;
- **vem desligado**: o switch da página liga, e para ligar é preciso definir uma senha
  (mín. 8). O console UART mantém `chiconws` para recuperação;
- troque na página IoT (mínimo 8 caracteres) — o hash fica no `cfm ssh.pwd` e é reaplicado
  depois que o `cfmd`/`netctrl` reescreve o `/etc/passwd`;
- cole uma chave pública na página; ela vai para o `cfm ssh.key` e é escrita em
  `/var/root/.ssh/authorized_keys` no boot (a home do root é `/var/root`, porque `/` é
  somente leitura).

A linha de comando funciona normalmente:

```sh
ssh root@<ip>                 # senha
ssh -i ~/.ssh/chave root@<ip> # chave
```

## Grupo IoT por MAC

Não existe **faixa de IP** de IoT. O grupo é uma lista de MACs:

```sh
iot-clients add aa:bb:cc:dd:ee:ff
iot-clients del aa:bb:cc:dd:ee:ff
iot-clients list
iot-clients status
iot-clients clients          # ARP + leases
iot-internet on|off          # grupo inteiro
```

Como funciona: uma chain `CH_iot` do `iptables` é pendurada no `FORWARD`
(`-i br0 -j CH_iot`); cada MAC listado ganha uma regra `DROP`. Um vigia reaplica a chain a
cada 30 s porque o `netctrl` reescreve o firewall em eventos de rede. A lista e o flag
ligado/desligado ficam no CMF (`iot.clients`, `iot.internet`), então sobrevivem a reboot.

Como casa pelo **MAC de origem**, cliente no cabo (atrás de switch não gerenciável) e no
Wi-Fi são tratados igual. É **só L3**: bloqueia a internet, não o tráfego entre dois
clientes da mesma LAN.

## Página na webui ("IoT")

O slot do menu TR-069 agora é **IoT** (mesmo visual das outras páginas: botões verdes,
tema escuro). A rota é `#/iot`.

- **Clientes**: IP(s) + MAC + grupo (IoT/LAN) + botão para alternar. As entradas são
  agrupadas por MAC (a tabela ARP pode ter vários IPs do mesmo aparelho).
- **Senha do SSH**: define uma nova senha do root.
- **Chave pública**: cadastrar / remover uma chave.
- **Faixa de status**: RAM usada/total (%), térmico 2.4G/5G (ADC relativo, não °C) e
  uptime.
- **Rodapé** (em toda a UI e no login): versão do Twibify, link do repo e uma linha de
  status ao vivo, atualizada a cada segundo.

## Comportamento do sistema

| Chave | Valor | Significado |
|---|---|---|
| `cwmpd.en` | `0` | TR-069 desligado (binário também removido) |
| `ucloud.en` | `1` | nuvem **mantida** — o app continua gerenciando o mesh |
| `sys.itbupgrade.enable` | `0` | OTA desligado (URL esvaziada) |
| `sys.schedulereboot.enable` | `0` | reboot diário desligado |
| `sched.reboot.enable` | `0` | o segundo agendador, desligado |
| `adv.upnp.en` | o que você definir | não é alterado pelo firmware |

Os valores são forçados pelo `rcS` via `cfm` porque o CMF sobrevive ao flash e manteria os
antigos. O telnet foi removido (link `telnetd`), então `/goform/telnet` falha.

## API HTTP (porta 8080)

A página conversa com um backend pequeno na `:8080`, autenticado por um **token por boot**
escrito em `/webroot/iot.token` (só entregue para sessão logada na web):

| Endpoint | Conteúdo |
|---|---|
| `GET /api/status` | versão, estado da chave, RAM, térmico, uptime |
| `GET /api/clients` | clientes do ARP + flag IoT |
| `POST /api/iot` | `{"mac":"..","iot":true/false}` |
| `GET /api/ssh` | `{"enabled":..,"key":..}` |
| `POST /api/ssh/enable` | `{"password":".."}` — define a senha e sobe o dropbear |
| `POST /api/ssh/disable` | derruba o dropbear (`ssh.enabled=0`) |
| `POST /api/ssh/password` | `{"password":".."}` |
| `POST /api/ssh/key` | `{"key":"ssh-ed25519 ..."}` |
| `DELETE /api/ssh/key` | remove a chave |

```sh
TOKEN=$(curl -s -b cookie http://<ip>/iot.token)
curl -H "Authorization: Bearer $TOKEN" http://<ip>:8080/api/status
```
