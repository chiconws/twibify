# Firmware

[English](README.md) | **Português**

| Arquivo | Tamanho | SHA-256 |
|---|---|---|
| `twibify-v1.0.bin` | 6.796.372 B | `028a4178da83b1c3f126a78ee655b98302aa2f28743af5c121dcdefec78a2de1` |

Formato: **uImage (header de 64 bytes) + kernel (stock 1.1.11) + rootfs squashfs +
trailer**, montado pelo `tools/mkimage_twibi.py` com o checksum do rootfs recalculado.

## Instalação

O bootloader aceita a imagem **sem os 64 bytes do cabeçalho**:

```sh
python3 -c "d=open('twibify-v1.0.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"

# aparelho em disaster mode, PC em 192.168.1.0/24:
python3 ../tools/tftp_put.py 192.168.1.6 payload.bin --name payload.bin
```

Procedimento completo: [`../docs/install.pt-BR.md`](../docs/install.pt-BR.md).

## Origem

- O **kernel** e o **rootfs base** são propriedade da Intelbras/Tenda; esta imagem é um
  derivado distribuído para **estudo e uso pessoal no próprio aparelho**.
- Não há dados pessoais: é uma imagem de firmware, não um dump da flash (sem MACs, sem
  chaves de Wi-Fi, sem configuração).
- A partição de configuração (CFM) **não** faz parte da imagem, então os ajustes de rede e
  senha continuam como estão ao gravar.
