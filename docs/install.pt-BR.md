# Instalação e rollback

[English](install.md) | **Português**

O Twibify é gravado pelo **disaster mode** do bootloader, não pela webui (o upgrade local
do original devolve `1005` até para a imagem oficial — veja o repo de RE).

## O que você precisa

- o Twibi Fast (qualquer 1.1.x de fábrica; o kernel é mantido);
- um cabo Ethernet do PC até uma porta LAN;
- cliente TFTP (`tools/tftp_put.py`, feito com `tftpy`);
- opcional: adaptador FTDI de 3,3 V nos pads da UART para acompanhar o boot (115200 8N1).

## Antes

A gravação troca apenas **kernel + rootfs**. O bootloader, as áreas de MAC/rótulo, o
**CFM** (configuração) e a partição de log não são tocados — então o IP da LAN, o Wi-Fi e a
senha de admin continuam como estão. Guarde uma cópia do `twibi_fast_1.1.11.bin` (oficial)
para rollback.

## Gravar

1. Desligue o aparelho.
2. Segure o **reset**, ligue **ainda segurando** e mantenha por ~15 s; solte. O console
   imprime `Entry disaster mode` e o bootloader sobe o TFTP em `192.168.1.6`.
3. Coloque o PC em `192.168.1.0/24` (ex.: `192.168.1.5/24`).
4. Remova os 64 bytes do cabeçalho e envie:

```sh
python3 -c "d=open('twibify-v1.0.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"
python3 tools/tftp_put.py 192.168.1.6 payload.bin --name payload.bin
```

O bootloader responde:

```
**TFTP Client Upload File Size = ... Success!
Linux kernel (root-fs) upgrade.
checksum Ok !
burn kernel-fs ...
squashfs upgrade. checksum Ok !
burn rootfs ...
```

Ele reinicia sozinho (~60–90 s até a webui responder).

## Primeiro boot

- A LAN mantém o endereço que tinha (o CFM sobrevive). O login é a senha de admin
  existente.
- SSH: **desligado por padrão**. Abra a página **IoT**, defina uma senha e clique em
  **Ligar SSH**. O console UART continua com `chiconws`.
- A página IoT fica em "Configurações avançadas → IoT".

## Se não bootar

O bootloader está intacto — volte ao disaster mode e grave a imagem oficial
(`twibi_fast_1.1.11.bin`, removendo o cabeçalho do mesmo jeito). A UART não é necessária
para a transferência; só ajuda a ver o que aconteceu.

## Rollback para o stock

Mesmo procedimento com a imagem oficial do
[`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re).
Um factory reset depois disso volta à configuração de fábrica.
