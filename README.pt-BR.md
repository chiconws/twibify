# Twibify

[English](README.md) | **Português**

Uma modificação mínima e reversível do firmware do **Intelbras Twibi Fast** (OEM Tenda,
Realtek RTL8197F): **SSH**, **grupo IoT por MAC** e uma página de gerenciamento **dentro
da webui original** — mantendo a nuvem, para o app continuar gerenciando o mesh.

Feito sobre o rootfs oficial **1.1.11**, com o kernel e as partições de fábrica intocados.

> ⚠️ Projeto pessoal no meu próprio aparelho. Não é afiliado à Intelbras nem à Tenda.
> Gravar firmware **anula a garantia** e pode inutilizar o aparelho; o disaster mode do
> bootloader é a rede de segurança. Use por sua conta e risco.

---

## Por quê

O firmware original é bom — exceto por expor **telnet**, manter **TR-069** e nuvem
ligados, reiniciar todo dia às 03:00 e não ter controle de internet por cliente. O
Twibify muda só isso e adiciona o que faltava:

| Recurso | Original | Twibify |
|---|---|---|
| Acesso remoto | telnet sob demanda | **SSH** (dropbear, senha + chaves públicas) |
| TR-069 (`cwmpd`) | ativo | **desligado** (binário removido) |
| Nuvem (`ucloud`) | ativo | **mantida** — o app continua gerenciando o mesh |
| OTA (`sys.itbupgrade`) | ativo | **desligado** (URL esvaziada) |
| Reboot diário 03:00 | ativo | **desligado** (as duas chaves) |
| IoT | — | **grupo por MAC**: internet liga/desliga por cliente |
| UI | — | **página IoT & SSH** dentro da webui original (slot do TR-069) |
| Branding | Intelbras | Intelbras + rodapé "firmware paralelo" com versão e repo |

## Recursos

### SSH

- dropbear estático na porta 22, iniciado pelo `rcS`;
- **senha inicial: `chiconws`** (root) — troque na página IoT;
- chaves públicas: cole uma na página; ela fica no `cfm` e é aplicada em
  `/var/root/.ssh/authorized_keys` no boot;
- autenticação por senha e por chave funcionam juntas.

### Grupo IoT por MAC

Não existe **faixa fixa de IP** para IoT — isso colidia com o endereçamento dos nodes do
mesh. O grupo é uma lista de **MACs**, então vale igual para cliente no cabo (mesmo atrás
de switch) e no Wi-Fi:

```sh
iot-clients add aa:bb:cc:dd:ee:ff
iot-clients del aa:bb:cc:dd:ee:ff
iot-clients list
iot-internet on|off      # o grupo inteiro
```

A chain `CH_iot` do `iptables` (no `FORWARD` do `br0`) derruba a saída para a WAN de cada
MAC listado. Um vigia reaplica a cada 30 s porque o `netctrl` reescreve o firewall.

### Página na webui

O slot da página TR-069 agora tem **IoT & SSH** (mesmo menu, mesmo visual: botões verdes,
tema escuro):

- lista de clientes (sem duplicar por MAC) com botão de IoT;
- formulário de senha do SSH e campo de chave pública;
- faixa de status: RAM, térmico 2.4G/5G (ADC relativo), uptime;
- o rodapé mostra a versão do firmware + **status ao vivo** (atualiza a cada segundo).

## Instalação

A imagem é [`firmware/twibify-v1.0.bin`](firmware/twibify-v1.0.bin) (formato uImage,
6.796.372 bytes). O bootloader aceita a imagem **sem os 64 bytes do cabeçalho**, então
remova e envie por TFTP no disaster mode:

```sh
python3 -c "d=open('twibify-v1.0.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"

# aparelho em disaster mode, PC em 192.168.1.0/24:
python3 tools/tftp_put.py 192.168.1.6 payload.bin --name payload.bin
```

Para entrar em disaster mode: desliga, segura o **reset**, liga e mantém por ~5 s. O
bootloader imprime `Entry disaster mode` e sobe um servidor TFTP em `192.168.1.6`.

**Rollback**: o mesmo procedimento com o `twibi_fast_1.1.11.bin` oficial (veja
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re)).
O bootloader nunca é tocado.

Passo a passo: [`docs/install.pt-BR.md`](docs/install.pt-BR.md).

## Notas e limites

- **Sem isolamento L2 por cliente**: o driver do switch expõe VLANs (`/proc/net/vlan`) e a
  bridge do `eth0.9` funciona, mas a porta LAN não entra em outra VLAN, não existe VLAN
  por MAC e o kernel 3.10 não tem bridge filtering/ebtables. O grupo IoT é **L3**
  (internet bloqueada; LAN acessível).
- **Mesh com mestre modificado** ainda não foi testado com um node stock; o stack do mesh
  está intocado e a base é a mesma 1.1.11, então espera-se que pareie.
- **O upgrade local da web original é stub**: `POST /goform/upgrade` devolve
  `{"errcode":"1005"}` até para a imagem oficial, então atualiza-se por disaster mode (ou
  pelo OTA da nuvem, que instala o firmware oficial — deixe desligado).
- A **senha de admin da web** não é alterada por este firmware; fica a que o aparelho tem.

## Estrutura do repositório

```
docs/        instalação, recursos, arquitetura e build
firmware/    twibify-v1.0.bin + instruções
tools/       mkimage_twibi.py (checksum/imagem) e tftp_put.py (disaster mode)
images/      foto da placa
```

## Créditos

Desenvolvido com o **DeepSeek** — engenharia reversa, build do firmware, backend, UI e
documentação. As notas de engenharia reversa ficam em
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re).

## Licença

MIT — veja [`LICENSE`](LICENSE). A imagem de firmware é um **derivado do firmware da
Intelbras** (mesmo kernel e rootfs base); é distribuída para estudo e uso pessoal no
próprio aparelho.
