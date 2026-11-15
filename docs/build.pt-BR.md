# Build

[English](build.md) | **Português**

A imagem é o rootfs oficial 1.1.11 com as mudanças listadas em
[`architecture.pt-BR.md`](architecture.pt-BR.md). Tudo o que é preciso para reconstruir
(menos a imagem oficial) está neste repositório.

## Requisitos

- Linux com `unsquashfs`/`mksquashfs`;
- Python 3 + [`tftpy`](https://pypi.org/project/tftpy/) (`pip install tftpy`);
- `gcc-mipsel-linux-gnu` para o backend (`apt install gcc-mipsel-linux-gnu` no
  Debian/Ubuntu);
- os binários estáticos do dropbear/dropbearkey (mipsel, musl) — os mesmos do projeto
  anterior; ou compile você mesmo;
- o `twibi_fast_1.1.11.bin` oficial do
  [`chiconws/intelbras-twibi-fast-re`](https://github.com/chiconws/intelbras-twibi-fast-re)
  (também usado como template do header uImage).

## 1. Extrair kernel e rootfs

O `.bin` oficial é `header uImage (64 B) + kernel (lzma) + squashfs + trailer`. O repo de
RE tem `tools/unpack_kernel.py` para o kernel; o rootfs começa em `0x40 + tamanho do
kernel` (0x1ED412 no 1.1.11).

```sh
python3 ../intelbras-twibi-fast-re/tools/unpack_kernel.py twibi_fast_1.1.11.bin
# kernel.lzma + rootfs.squashfs
unsquashfs -d rootfs rootfs.squashfs
```

## 2. Aplicar as mudanças

| Item | Origem neste repo |
|---|---|
| `usr/sbin/dropbear`, `usr/bin/dropbearkey` | binários estáticos (não incluídos) |
| `usr/sbin/iotweb` | compilado de `src/iotweb.c` + `src/md5crypt.c` |
| `usr/sbin/iot-clients` | `src/iot-clients` (chmod 755) |
| `webroot_ro/iot.html` | `src/iot.html` |
| `webroot_ro/pages/tr069.js` | `src/tr069-chunk.js` |
| `etc_ro/init.d/rcS` | anexar `src/rcS.twibify` |
| `webroot_ro/default.cfg` | `cwmpd.en=0`, `sys.itbupgrade.enable=0` + URL vazia, `sys.schedulereboot.enable=0`, `sched.reboot.enable=0` |
| `webroot_ro/pages/app.js` | menu/rota `tr069` → `iot` |
| `webroot_ro/index.html`, `login.html` | rodapé com versão/link/status |
| `etc_ro/passwd`, `etc_ro/shadow` | hash `$1$` do root `chiconws`, home `/var/root` |
| `etc_ro/issue`, `etc_ro/chiconws_version` | banner e versão |

Gere o hash da senha do root com o mesmo código do backend:

```sh
gcc -O2 -DMD5CRYPT_MAIN -o md5crypt src/md5crypt.c
./md5crypt chiconws chiconws        # -> $1$chiconws$...
```

Depois coloque `root:<hash>:0:0:root:/var/root:/bin/sh` no `etc_ro/passwd` e
`root:<hash>:14319::::::` no `etc_ro/shadow`. Remova `usr/sbin/telnetd` e `bin/cwmpd`.

## 3. Compilar o backend

```sh
mipsel-linux-gnu-gcc -static -Os -s -o rootfs/usr/sbin/iotweb src/iotweb.c
```

(o `iotweb.c` inclui o `md5crypt.c`, então mantenha os dois no mesmo diretório.)

## 4. Reempacotar e montar a imagem

```sh
mksquashfs rootfs rootfs.squashfs -noappend -comp xz -b 128K
python3 tools/mkimage_twibi.py \
  --kernel kernel.lzma --rootfs rootfs.squashfs \
  --out twibify.bin --template twibi_fast_1.1.11.bin
```

A ferramenta recalcula o checksum do rootfs que o bootloader valida. Confira a soma dando
zero e grave:

```sh
python3 -c "d=open('twibify.bin','rb').read(); open('payload.bin','wb').write(d[0x40:])"
python3 tools/tftp_put.py 192.168.1.6 payload.bin
```

## Notas

- Mantenha o **kernel** da imagem oficial — só o rootfs muda.
- A imagem tem que caber na região kernel+rootfs (7.569.408 B em `0x30000–0x7C0000`); o
  Twibify 1.0 usa 6.796.308 B.
- Se o squashfs crescer, remova pacotes não usados do rootfs original antes de
  reempacotar.
