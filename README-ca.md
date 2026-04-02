# Beastie-Boy
Aquest projecte té com a objectiu desenvolupar dues eines, _beastie_ i _boy_. La primera volca les dades en memòria compartida amb [VPP](https://fd.io/) a través de `memif`, un tipus d'interfície de memòria directa de molt alt rendiment que es pot utilitzar entre instàncies de FD.io VPP. Posteriorment, genera un arxiu de captura dels paquets o trames intercanviats per una interfície de xarxa en mode _span/mirror_. La segona permet configurar VPP per activar el mode _span/mirror_ a través de la seva API.

## Compilació
El projecte usa un esquema de compilació similar a VPP: un `Makefile` orquestra la compilació, però el backend real és `CMake` amb `Ninja`, i el producte de la compilació es genera a `build-root/`. Per compilar la versió `release`:
```bash
$ make build
```
Els binaris i fitxers intermedis queden a:

* `build-root/build-beastie-boy-release-native/`
* `build-root/install-beastie-boy-release-native/`

La versió del codi de `beastie` i `boy` s’obté directament del repositori Git amb `git describe --tags --always --dirty`, de manera que queda basada en `tag + commit` quan hi ha tags disponibles. La podeu consultar amb:
```bash
$ build-root/build-beastie-boy-release-native/beastie --version
$ build-root/build-beastie-boy-release-native/boy --version
```
Si estem duent a terme tasques de depuració i volem activar _flags_ de símbols de depurat amb poques optimitzacions:
```bash
$ make build-debug
```
Per instal·lar els binaris sota el prefix configurat per CMake:
```bash
$ make install
```
Per generar paquets de Debian:
```bash
$ make package
$ make package-deb
```
Els arxius generats es desen a `build-root/build-beastie-boy-<tag>-native/`. Per netejar i reconfigurar:
```bash
$ make clean
$ make configure
```
## boy
Aquesta és l'eina que permet configurar la funció _span/mirror_ de VPP. També permet obtenir informació sobre les interfícies físiques i les que suporten _span/mirror_. Totes les accions que du a terme _boy_ a través de l'API de VPP no són permanents. El primer és crear una interfície `memif`.
```bash
$ boy -c
[INFO] boy starting
[INFO] created memif: id=0 socket-id=0 ring-size=1024 buffer-size=2048 sw_if_index=17
```
També es poden crear més interfícies `memif` indicant paràmetres específics:
```bash
$ boy --create-memif --id 1 --socket-id 0
```
Per mostrar el detall de la interfície creada:
```bash
$ boy -m
[INFO] boy starting
Interface Name  Socket-ID  Flags
--------------  ---------  -----
memif0/0                0  none
```
Per mostrar els _mapping_ d'interfícies i funció de _span/mirror_:
```bash
$ boy -s
[INFO] boy starting
Interface Name                If-Index  If-Type   Dev-Type  Destination  Device  L2
----------------------------  --------  --------  --------  -----------  ------  --
TenGigabitEthernet1/0/0              1  hardware  dpdk      *            *       * 
TenGigabitEthernet1/0/1              2  hardware  dpdk      *            *       * 
TenGigabitEthernet1/0/2              3  hardware  dpdk      *            *       * 
TenGigabitEthernet1/0/3              4  hardware  dpdk      *            *       * 
```
Si volem activar _span/mirror_ sobre la interfície `TenGigabitEthernet1/0/1` en les dues direccions (tx+rx), ens fixem en l'índex d'interfície (`If-Index`):
```bash
$ boy -S --iface-idx 2 --memif memif0/0 --device both
```
Verifiquem la configuració aplicada:
```bash
$ boy -s
[INFO] boy starting
Interface Name                If-Index  If-Type   Dev-Type  Destination  Device  L2
----------------------------  --------  --------  --------  -----------  ------  --
TenGigabitEthernet1/0/0              1  hardware  dpdk      *            *       * 
TenGigabitEthernet1/0/1              2  hardware  dpdk      memif0/0     both    * 
TenGigabitEthernet1/0/2              3  hardware  dpdk      *            *       * 
TenGigabitEthernet1/0/3              4  hardware  dpdk      *            *       *
```
Finalment, podem retirar la funció _span/mirror_:
```bash
$ boy -u 2
[INFO] boy starting
[INFO] removed 1 SPAN entry for source if-index=2 (TenGigabitEthernet1/0/1)
```

## beastie
Aquesta és l'eina que permet capturar els paquets que arriben a una interfície `memif` i els desa en un fitxer `PCAP`. Normalment s'utilitza després d'haver creat una `memif` amb `boy` i d'haver activat un _span/mirror_ cap a aquella interfície dins de VPP. `beastie` es connecta al Unix domain socket indicat amb `--socket` o, si no se n'indica cap, a `/run/vpp/memif.sock`. Dins d'aquest socket selecciona la interfície `memif` a partir de l'`id` indicat. Per defecte, es connecta com a extrem `slave` a la `memif` amb `id=0` i al socket `/run/vpp/memif.sock`, i escriu la captura a `capture.pcap`.
```bash
$ beastie
[INFO] beastie starting
[INFO] capture output: capture.pcap
[INFO] waiting for packets on memif...
```
Si volem desar la captura en un altre fitxer:
```bash
$ beastie --write trace.pcap
```
Podem limitar la captura a una mida concreta, per exemple 1 MiB:
```bash
$ beastie --write trace.pcap --max-bytes 1048576
```
Si la `memif` de VPP s'ha creat amb un identificador diferent, cal indicar-lo amb `--id`:
```bash
$ beastie --id 1 --write trace.pcap
```
Si VPP utilitza un socket diferent del predeterminat, també es pot indicar explícitament:
```bash
$ beastie --socket /run/vpp/custom.sock --write trace.pcap
```
Un flux típic de treball seria aquest:
```bash
$ boy --create-memif --id 1 --socket-id 0
$ boy -S --iface-idx 2 --memif memif0/1 --device both
$ beastie --id 1 --write trace.pcap
```
Amb `Ctrl+C` es pot iniciar l'aturada de la captura, `beastie` tanca correctament el fitxer `PCAP` i mostra un resum amb els bytes i paquets capturats.

# Apèndix A: Instal·lació de VPP
Compilarem la versió estable de VPP 26.02 i en crearem un paquet per a Debian. Partirem de la hipòtesi que en el sistema no hi ha ni VPP ni DPDK instal·lats.

Instal·lem primer els paquets mínims per compilar-lo. Totes les comandes s'executen de manera implícita com a _root_:
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
Atés que _beastie_ necessita la llibreria `libmemif.so`, la compilem:
```bash
$ cd /root/vpp/extras/libmemif
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
$ make -j"$(nproc)"
```
Dimensionem els _hugepages_ segons el maquinari. Consulteu els apèndixs B i C per obtenir informació sobre el cas d'APU3 i ASUSTeK P10S-I. Instal·lem tots els paquets que acabem de generar. Amb això ens assegurem que, en particular, la llibreria `libmemif.so` hi serà:
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

# Apèndix B: Plataforma de desenvolupament APU3 amb Debian 13
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
`flashrom` cal que tingui drets d'escriptura al dispositiu `/dev/mem`. Els _kernels_ moderns acostumen a bloquejar l'accés a aquest dispositiu fins i tot per a l'usuari _root_. Per permetre-ho cal modificar `/etc/default/grub`. Sota la directiva `GRUB_CMDLINE_LINUX_DEFAULT` cal incloure l'opció `iomem=relaxed`. Actualitzem la configuració de GRUB amb `update-grub`.

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
Proposem el següent arxiu `etc/sysctl.d/80-vpp.conf`:
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
# Apèndix C: Configuració d'ASUSTeK P10S-I amb Debian 13
Disposem d'un altre maquinari. Es tracta d'un servidor amb una placa base ASUSTeK, P10S-I Series, BIOS American Megatrends Inc. versió 4602, 32GB de RAM i un processador Intel(R) Xeon(R) CPU E3-1220 v5 @ 3.00GHz. Aquest servidor està equipat amb dues targetes de xarxa integrades Intel I210 i una targeta PCIe Intel X710, de quatre ports SFP+.

Per tal d'optimitzar aquest maquinari per treballar amb VPP, modifiquem els següents paràmetres de BIOS (Advanced): 

| CATEGORIA | OPCIÓ DE LA BIOS | CONFIGURACIÓ | 
| ----- | ----- | ----- | 
| Virtualització i Bus PCIe | Intel Virtualization Technology (VT-x) | [Enabled] | 
| Virtualització i Bus PCIe | PCI Latency Timer | [32 PCI clock] | 
| Gestió d'Energia i Freqüència | C-States (CPU Power Management) | [Disabled] | 
| Gestió d'Energia i Freqüència | Intel SpeedStep | [Disabled] | 
| Gestió d'Energia i Freqüència | Intel Speed Shift Technology | [Disabled] | 
| Gestió d'Energia i Freqüència | Turbo Mode (Turbo Boost) | [Disabled] | 
| Gestió d'Energia i Freqüència | DMI Link ASPM Control | [Disabled] | 
| Rendiment del Processador | Hardware Prefetcher | [Enabled] | 
| Rendiment del Processador | Adjacent Cache Line Prefetch | [Enabled] | 
| Rendiment del Processador | CPU AES (AES-NI) | [Enabled] | 
| Funcions de Seguretat | SW Guard Extensions (Intel SGX) | [Disabled] | 
| Funcions de Seguretat | Intel TXT Support | [Disabled] |

Procedim a una instal·lació de Debian 13 similar al de l'APU3. En aquest cas, no cal un accés a port sèrie.

Afegim una entrada a `/etc/default/grub`:
```
GRUB_CMDLINE_LINUX_DEFAULT="quiet intel_iommu=on iommu=pt isolcpus=1-3 nohz_full=1-3 rcu_nocbs=1-3 processor.max_cstate=1 intel_idle.max_cstate=1"
```
El que fa és:
* `intel_iommu=on iommu=pt`: Activa l'IOMMU (VT-d) i el posa en mode "pass-through" (pt). Això dóna accés directe i exclusiu de la targeta de xarxa al mòdul `vfio_pci`
* `isolcpus=1-3`: Diu a l'OS que no enviï cap tasca normal als nuclis 1, 2 i 3. A `/etc/vpp/startup.conf` li direm a VPP que usi els cores 2 i 3 per processar paquets i l'1 per la resta de tasques 
* `nohz_full=1-3 i rcu_nocbs=1-3`: Elimina les interrupcions de rellotge i de manteniment del sistema operatiu sobre aquests nuclis
* `processor.max_cstate=1 intel_idle.max_cstate=1`: És una capa extra de seguretat per forçar que Linux no intenti adormir els nuclis (complementant el que ja vam fer a la BIOS)

Apliquem els canvis:
```bash
$ update-grub
```
Modificarem el servei de `vpp` per tal que carregui el mòdul `vfio_pci` en lloc del `uio_pci_generic`. Debian 13 incorpora un nou mètode per modificar els serveis, de manera que les actualitzacions no els sobreescriguin. Executem:
```bash
$ systemctl edit vpp.service
```
S'obrirà un editor amb el contingut actual de l'arxiu comentat. Cal que el modifiquem en una zona concreta de l'arxiu. Hi afegim aquestes línies per eliminar l'anterior entrada `ExecStartPre` i modificar-ne el seu valor:
```
[Service]
ExecStartPre=
ExecStartPre=-/sbin/modprobe vfio_pci
```
Atés que la configuració que farem servir inclou un arxiu de log a `/var/log/vpp/vpp.log`, el creem perquè, per defecte, la instal·lació de VPP no el crea. Apliquem els canvis i reiniciem VPP:
```bash
$ systemctl daemon-reload
$ systemctl restart vpp
```
Si observem el log del servei, després de la fase de càrrega de plugins hauríem de veure alguna cosa similar a aquesta:
```
Apr 01 17:15:21 bboy vpp[986]: vpp[986]: dpdk: EAL: Detected CPU lcores: 4
Apr 01 17:15:21 bboy vpp[986]: dpdk: EAL: Detected CPU lcores: 4
Apr 01 17:15:21 bboy vpp[986]: vpp[986]: dpdk: EAL: Detected NUMA nodes: 1
Apr 01 17:15:21 bboy vpp[986]: vpp[986]: dpdk: EAL: Detected static linkage of DPDK
Apr 01 17:15:21 bboy vpp[986]: vpp[986]: dpdk: EAL: Selected IOVA mode 'VA'
Apr 01 17:15:21 bboy vpp[986]: vpp[986]: dpdk: EAL: No free 1048576 kB hugepages reported on node 0
Apr 01 17:15:21 bboy vpp[986]: vpp[986]: dpdk: EAL: VFIO support initialized
Apr 01 17:15:21 bboy vpp[986]: vpp[986]: dpdk: EAL: Using IOMMU type 1 (Type 1)
Apr 01 17:15:21 bboy vpp[986]: dpdk: EAL: Detected NUMA nodes: 1
Apr 01 17:15:21 bboy vpp[986]: dpdk: EAL: Detected static linkage of DPDK
Apr 01 17:15:21 bboy vpp[986]: dpdk: EAL: Selected IOVA mode 'VA'
Apr 01 17:15:21 bboy vpp[986]: dpdk: EAL: No free 1048576 kB hugepages reported on node 0
Apr 01 17:15:21 bboy vpp[986]: dpdk: EAL: VFIO support initialized
Apr 01 17:15:21 bboy vpp[986]: dpdk: EAL: Using IOMMU type 1 (Type 1)
Apr 01 17:15:28 bboy vpp[986]: vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 0).
Apr 01 17:15:28 bboy vpp[986]: vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 2).
Apr 01 17:15:28 bboy vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 0).
Apr 01 17:15:28 bboy vpp[986]: vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 3).
Apr 01 17:15:28 bboy vpp[986]: vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 1).
Apr 01 17:15:28 bboy vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 2).
Apr 01 17:15:28 bboy vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 3).
Apr 01 17:15:28 bboy vpp[986]: dpdk: I40E_DRIVER: i40e_set_rx_function(): Using Vector AVX2 Scattered (port 1).
```
Una línia clau és `Using IOMMU type 1 (Type 1)`. Això confirma que el nucli de Debian ha reconegut la configuració de la BIOS (VT-d) i que el mòdul `vfio_pci` té el control total del maquinari. El `Type 1` és el mode més segur i eficient de fer un pass-through de targetes PCIe a Linux. L'altra és `Using Vector AVX2 Scattered`. Apareix gràcies a que hem activat el _Hardware Prefetcher_ i el _Adjacent Cache Line Prefetch_ a la BIOS. `Vector AVX2` significa que VPP està utilitzant les instruccions SIMD (_Single Instruction Multiple Data_) per processar paquets en blocs. `Scattered` indica que el controlador DPDK (i40e per a les targetes Intel X710) pot gestionar paquets que estiguin fragmentats a la memòria de forma molt ràpida.

## Dimensionat de _hugepages_
Proposem el següent arxiu `etc/sysctl.d/80-vpp.conf`:
```
# VPP on medium/large systems (32 GB RAM, 4 Cores)
# Generous values for high throughput and stable memory allocation.

# Reserve hugepages for DPDK/VPP (2 MB pages)
# 4096 * 2 MB = 8 GB reserved for hugepages. 
# (Augmentat per suportar més trànsit i taules BGP grans sense ofegar els 32GB)
vm.nr_hugepages = 4096

# Allow large shared-memory and hugepage-backed mappings
# Doblat per evitar límits en entorns amb moltes interfícies o contenidors
vm.max_map_count = 524288

# Keep swapping low; VPP/DPDK hate latency spikes
vm.swappiness = 10

# Leave some room before the kernel starts reclaim pressure
# 512 MB de matalàs lliure per evitar aturades d'assignació al kernel (estava a 128MB)
vm.min_free_kbytes = 524288

# Permit enough shared memory for VPP segments, memif, stats, etc.
# Permetem fins a 16 GB de memòria compartida (aprox la meitat de la RAM)
kernel.shmmax = 17179869184
kernel.shmall = 4194304

# Larger socket buffers help control-plane/API tools and capture paths
# Augmentem els màxims a 64 MB per suportar grans ràfegues
net.core.rmem_default = 524288
net.core.rmem_max = 67108864
net.core.wmem_default = 524288
net.core.wmem_max = 67108864

# Backlog for bursts
# Molt més espai per a ràfegues, adequat per a una màquina que pot processar ràpid (4 cores)
net.core.netdev_max_backlog = 65536

# More room for local sockets used by agents/tools around VPP
net.unix.max_dgram_qlen = 2048
```
i per `etc/sysctl.d/81-vpp-netlink.conf`:
```
# Netlink tuning for VPP integration on a robust host.
# Ample headroom for massive interface churn, large BGP routes, neighbors and startup sync.

net.core.rmem_default = 524288
net.core.rmem_max = 67108864
net.core.wmem_default = 524288
net.core.wmem_max = 67108864

# Help user space drain bursts of rtnetlink events
net.core.netdev_max_backlog = 65536
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
