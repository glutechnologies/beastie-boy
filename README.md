# Beastie-Boy
Aquest projecte té objectiu desenvolupar dues eines _beastie_ i _boy_. La primera volca les dades en memòria compartida amb [VPP](https://fd.io/) a través de `memif`, un tipus d'interfície de memòria directa de molt alt rendiment que es pot utilitzar entre instàncies de FD.io VPP. Posteriorment _beastie_ genera un arxiu de captura  dels paquets o trames intercanviades per una interfície de xarxa en mode _span/mirror_. La segona permet configurar VPP per activar el mode _span/mirror_ a través de la seva API.

## Compilació
El projecte incorpora un script `./configure` personalitzat per validar dependències abans de compilar i generar `config.mk`. Fa comprovacions de _headers_ i enllaç per `pcap`, `memif` i `vapi`, i també detecta la versió de VPP suportada.

Iniciem el procés amb la configuració de l'entorn de compilació:

```bash
$ ./configure
```
Es generarà el fitxer `config.mk`, inclòs després pel `Makefile`, amb _flags_ detectats (`PCAP_*`, `MEMIF_*`, `VAPI_*`), metadades de compatibilitat VPP (`VPP_API_VERSION`, `VPP_API_SERIES`) i macros de compilació (`VPP_COMPAT_CPPFLAGS`). Diposa d'algunes opcions útils:

- `--quiet`: redueix la sortida
- `--supported-vpp-series=2506,2510,2602`: defineix la llista de sèries admeses
- `--allow-unsupported-vpp`: permet continuar si la sèrie detectada no és a la llista

La detecció de versió VPP es fa primer amb `pkg-config`; si no està disponible, fa _fallback_ amb `dpkg-query` (habitual en instal·lacions VPP via `.deb`).

Si tot ha anat bé, podem iniciar la compilació:

```bash
$ make
```
`make` de fet invoca `./configure` i compila els dos binaris `bin/beastie` i `bin/boy`. El `Makefile` actual és incremental i compila per objectes (`.o`), genera dependències automàtiques (`.d`) amb `-MMD -MP` i detecta canvis en `.c` i `.h` sense necessitat de `make clean`.

Si estem duent a terme tasques de depurat, podem compilar amb `-O0 -g`, és a dir, sense optimitzacions agressives:

```bash
$ make debug
```
Per reconfigurar, eliminar binaris, arxius objecte i `config.mk`:
```bash
$ make reconfigure
$ make clean
```

# Appendix A: Instal·lació de VPP
Compilar la versió estable de VPP 26.02 i crearem un paquet per Debian. Partirem de la hiopòtesi qyue en el sistema no hi ha vi VPP ni DPDK instal·lats.

Instal·lem primers els paquets mínims per compilar-lo. Totes les comandes s'executem de manera implícita sotya _root_:
```bash
$ apt update
$ apt install -y git make sudo cmake build-essential
```
Clonem el codi font des del dipòsit oficial:
```bash
$ git clone https://gerrit.fd.io/r/vpp
$ cd vpp
$ git checkout v26.02
```
Afegim les dependències per la compilació, provem el _build_ i empaquetem VPP:
```bash
$ cd /root/vpp
$ make install-dep
$ make build-release
$ make pkg-deb
```
Atés que _beastie_ necessita la llibreria `libmemif.so`, la compilem i instal·lem :
```bash
$ cd /root/vpp/extras/libmemif
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
$ make -j"$(nproc)"
$ make install
```
Dimensionem els _hugepages_ segons el maquinari. Consulteu l'appendix B per obtenir informació sobre el cas d'APU3. Instal·lem tots els paquets que acabem de generar. Amb això ens assegurem que en particular, la lliberia `libmemif.so` hi serà:
```bash
$ cd /root/vpp/build-root
$ dpkg -i ./*.deb
```
Si no s'han instal·lat dependències per `dpkg`, podem forçar la seva instal·lació:
```bash
$ apt -f install
```
Instal·lem la llibreria `libmemif.so`:
```bash
$ cd /root/vpp/extras/libmemif/build
$ make install
```

# Appendix B: Plataforma de desenvolupament APU3 amb Debian 13
Disposem d'una PC Engines APU3D4 amb 3 LAN i211AT / CPU AMD GX-412TC / 4 GB de DRAM. Aquestes targetes de xarxa són compatibles amb DPDK. Per instal·lar Debian 13 cal descarregar una imatge `netinst.iso` de https://www.debian.org/CD/netinst/ i copiar-la a una unitat USB:

```
$ sudo dd if=debian-13.4.0-amd64-netinst.iso of=/dev/<sdX> bs=8M status=progress
$ sudo sync
```
Assegureu-vos que el dispositiu `/dev/sdX` és el que no està muntat: alguna cosa com `/dev/sdb` o `/dev/sdc`.
Feu servir `picocom` per obtenir un mode de visualització agradable i amb colors:
```
$ picocom -b 115200 /dev/ttyUSB0
Per enviar un caràcter TAB, envieu-lo en hexadecimal: C-a C-w ==> 09
Per sortir de `picocom`: C-a C-x
```
Premeu F10 per accedir al menú d'arrencada. Arrenqueu des de la unitat USB inserida. Després, premeu TAB per modificar els paràmetres d'arrencada:
```
-/install.amd/vmlinuz vga=788 initrd=/install.amd/gtk/initrd.gz --- quiet
+/install.amd/vmlinuz vga=off initrd=/install.amd/gtk/initrd.gz --- quiet console=ttyS0,115200u8
```
Seguiu el procediment habitual d'instal·lació de Debian.

## Actualització de BIOS i activació d'IOMMU a APU3
Atés que volem usar VPP amb el mòdul `vfio-pci` i aquest requereix IOMMU, caldrà que APU3 el suporti a nivell de BIOS. Plantegem doncs l'actualització de la BIOS amb una versió que ho suporta. Suposarem que ja tenim instal·lat un Linux/Debian. Podem fer-ho des d'aquest sistema operatiu.

Primer cal instal·lar `flashrom`:

```bash
$ apt update
$ apt install flashrom
```
`flashrom` cal que tingui drets d'escriptra al dispositiu `/dev/mem`. Els _kernels_ moderns acostumen a bloquejar d'accés a aquest dispotiu fins i tot per l'usuari _root_. Per permetre-ho cal modificar `/etc/default/grub`. Sota la directiva `GRUB_CMDLINE_LINUX_DEFAULT` cal incloure l'opció `iomem=relaxed`. Actualitzem les noves configuracions de GRUB amb `update-grub`.

Ara baixem el nou binary de la ROM. La gestió de IOMMU s'inclou a partir de la v4.11.0.2:
```bash
$ wget https://3mdeb.com/open-source-firmware/pcengines/apu3/apu3_v4.19.0.1.rom
```
I ara ja podem actualitzar. Les opcions de `flashrom` són importants:
```bash
$ flashrom -p internal:boardmismatch=force -c "W25Q64BV/W25Q64CV/W25Q64FV" -w apu3_v4.19.0.1.rom 
flashrom unknown on Linux 6.1.0-37-amd64 (x86_64)
flashrom is free software, get the source code at https://flashrom.org
Using clock_gettime for delay loops (clk_id: 1, resolution: 1ns).
coreboot table found at 0xdffae000.
Found chipset "AMD FCH".
Enabling flash write... OK.
Found Winbond flash chip "W25Q64BV/W25Q64CV/W25Q64FV" (8192 kB, SPI) mapped at physical address 0x00000000ff800000.
This coreboot image (PC Engines:apu3) does not appear to
be correct for the detected mainboard (PC Engines:PCEngines apu3).
Proceeding anyway because user forced us to.
Reading old flash chip contents... done.
Erasing and writing flash chip... Erase/write done.
Verifying flash... VERIFIED.
```
Un cop actualitzada la BIOS, habilitem la funció IOMMU. Accedim per port sèrie de l'APU3 al menú de BIOS i habilitem l'opció IOMMU:
```
2 Payload [setup] > Opció v > Opció s
```
Si tot va bé, el sistema hauria de mostrar aquests missatges de kernel:
```bash
$ sudo dmesg | grep IOMMU
[    0.743352] pci 0000:00:00.2: AMD-Vi: IOMMU performance counters supported
[    0.751367] pci 0000:00:00.2: AMD-Vi: Found IOMMU cap 0x40
[   22.300235] perf/amd_iommu: Detected AMD IOMMU #0 (2 banks, 4 counters/bank).
[   22.354872] AMD-Vi: AMD IOMMUv2 loaded and initialized
```
Si tenim VPP correctament instal·lat, podrem veure que detecta les interfícies de xarxa amb el driver `vfio-pci`:
```
vpp# show hardware-interfaces 
              Name                Idx   Link  Hardware
GigabitEthernet2/0/0               1    down  GigabitEthernet2/0/0
  Link speed: unknown
  RX Queues:
    queue thread         mode      
    0     vpp_wk_0 (1)   polling   
  TX Queues:
    TX Hash: [name: hash-eth-l34 priority: 50 description: Hash ethernet L34 headers]
    queue shared thread(s)      
    0     yes    0, 2
    1     no     1
  Ethernet address 00:0d:b9:56:18:35
  Intel e1000
    carrier down 
    flags: maybe-multiseg tx-offload intel-phdr-cksum rx-ip4-cksum
    Devargs: 
    rx: queues 1 (max 2), desc 512 (min 32 max 4096 align 8)
    tx: queues 2 (max 2), desc 512 (min 32 max 4096 align 8)
    pci: device 8086:1539 subsystem 8086:0000 address 0000:02:00.00 numa 0
    max rx packet len: 16383
    promiscuous: unicast off all-multicast off
    vlan offload: strip off filter off qinq off
    rx offload avail:  vlan-strip ipv4-cksum udp-cksum tcp-cksum vlan-filter 
                       vlan-extend scatter keep-crc rss-hash 
    rx offload active: ipv4-cksum scatter 
    tx offload avail:  vlan-insert ipv4-cksum udp-cksum tcp-cksum sctp-cksum 
                       tcp-tso multi-segs send-on-timestamp 
    tx offload active: ipv4-cksum udp-cksum tcp-cksum multi-segs 
    rss avail:         ipv4-tcp ipv4-udp ipv4 ipv6-tcp-ex ipv6-udp-ex ipv6-tcp 
                       ipv6-udp ipv6-ex ipv6 
    rss active:        none
    tx burst function: (not available)
    rx burst function: (not available)

GigabitEthernet3/0/0               2    down  GigabitEthernet3/0/0
  Link speed: unknown
  RX Queues:
    queue thread         mode      
    0     vpp_wk_1 (2)   polling   
  TX Queues:
    TX Hash: [name: hash-eth-l34 priority: 50 description: Hash ethernet L34 headers]
    queue shared thread(s)      
    0     yes    0, 2
    1     no     1
  Ethernet address 00:0d:b9:56:18:36
  Intel e1000
    carrier down 
    flags: maybe-multiseg tx-offload intel-phdr-cksum rx-ip4-cksum
    Devargs: 
    rx: queues 1 (max 2), desc 512 (min 32 max 4096 align 8)
    tx: queues 2 (max 2), desc 512 (min 32 max 4096 align 8)
    pci: device 8086:1539 subsystem 8086:0000 address 0000:03:00.00 numa 0
    max rx packet len: 16383
    promiscuous: unicast off all-multicast off
    vlan offload: strip off filter off qinq off
    rx offload avail:  vlan-strip ipv4-cksum udp-cksum tcp-cksum vlan-filter 
                       vlan-extend scatter keep-crc rss-hash 
    rx offload active: ipv4-cksum scatter 
    tx offload avail:  vlan-insert ipv4-cksum udp-cksum tcp-cksum sctp-cksum 
                       tcp-tso multi-segs send-on-timestamp 
    tx offload active: ipv4-cksum udp-cksum tcp-cksum multi-segs 
    rss avail:         ipv4-tcp ipv4-udp ipv4 ipv6-tcp-ex ipv6-udp-ex ipv6-tcp 
                       ipv6-udp ipv6-ex ipv6 
    rss active:        none
    tx burst function: (not available)
    rx burst function: (not available)
```
## Dimensionat de _hugepages_
Proposem els següents arxiu `etc/sysctl.d/80-vpp.conf`:
```
# VPP on small-memory systems (APU3, 4 GB RAM)
# Conservative values to avoid starving the OS.

# Reserve hugepages for DPDK/VPP (2 MB pages)
# 512 * 2 MB = 1 GB reserved for hugepages
vm.nr_hugepages = 512

# Allow large shared-memory and hugepage-backed mappings
vm.max_map_count = 262144

# Keep swapping low; VPP/DPDK hate latency spikes
vm.swappiness = 10

# Leave some room before the kernel starts reclaim pressure
vm.min_free_kbytes = 131072

# Permit enough shared memory for VPP segments, memif, stats, etc.
kernel.shmmax = 1073741824
kernel.shmall = 262144

# Larger socket buffers help control-plane/API tools and capture paths
net.core.rmem_default = 262144
net.core.rmem_max = 33554432
net.core.wmem_default = 262144
net.core.wmem_max = 33554432

# Backlog for bursts
net.core.netdev_max_backlog = 4096

# More room for local sockets used by agents/tools around VPP
net.unix.max_dgram_qlen = 512
```
i per `etc/sysctl.d/81-vpp-netlink.conf`:
```
# Netlink tuning for VPP integration on a small host.
# Enough headroom for interface churn, routes, neighbors and startup sync.

net.core.rmem_default = 262144
net.core.rmem_max = 33554432
net.core.wmem_default = 262144
net.core.wmem_max = 33554432

# Help user space drain bursts of rtnetlink events
net.core.netdev_max_backlog = 4096
```
Apliquem aquests arxius:
```bash
$ sysctl -p -f /etc/sysctl.d/80-vpp.conf
$ sysctl -p -f /etc/sysctl.d/81-vpp-netlink.conf
```
Si volem aplicar tots els arxius _system_ del sistema i verificar possibles conflictes:
```bash
$ sysctl --system
```
