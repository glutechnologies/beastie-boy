# Beastie-Boy
This project aims to develop two tools, _beastie_ and _boy_. The first one receives data through shared memory with [VPP](https://fd.io/) using `memif`, a very high-performance memory interface that can be used between FD.io VPP instances. It then generates a capture file containing the packets or frames exchanged by a network interface in _span/mirror_ mode. The second tool configures VPP to enable _span/mirror_ mode through its API.

## Build
The project uses a build scheme similar to VPP: a `Makefile` orchestrates the build, but the real backend is `CMake` with `Ninja`, and the build output is generated under `build-root/`. To build the `release` version:
```bash
$ make build
```
The binaries and intermediate files are placed in:

* `build-root/build-beastie-boy-release-native/`
* `build-root/install-beastie-boy-release-native/`

The code version for `beastie` and `boy` is obtained directly from the Git repository with `git describe --tags --always --dirty`, so it is based on `tag + commit` when tags are available. You can query it with:
```bash
$ build-root/build-beastie-boy-release-native/beastie --version
$ build-root/build-beastie-boy-release-native/boy --version
```
If we are performing debugging tasks and want to enable debug symbols with low optimization levels:
```bash
$ make build-debug
```
To install the binaries under the prefix configured by CMake:
```bash
$ make install
```
To generate Debian packages:
```bash
$ make package
$ make package-deb
```
The generated files are stored in `build-root/build-beastie-boy-<tag>-native/`. To clean and reconfigure:
```bash
$ make clean
$ make configure
```

## boy
This tool configures VPP _span/mirror_ functionality. It also provides information about physical interfaces and interfaces that support _span/mirror_. All actions performed by _boy_ through the VPP API are non-persistent. The first step is to create a `memif` interface.
```bash
$ boy -c
[INFO] boy starting
[INFO] created memif: id=0 socket-id=0 ring-size=1024 buffer-size=2048 sw_if_index=17
```
Additional `memif` interfaces can also be created by specifying explicit parameters:
```bash
$ boy --create-memif --id 1 --socket-id 0
```
To show the details of the created interface:
```bash
$ boy -m
[INFO] boy starting
Interface Name  Socket-ID  Flags
--------------  ---------  -----
memif0/0                0  none
```
To display interface mappings and _span/mirror_ state:
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
If we want to enable _span/mirror_ on interface `TenGigabitEthernet1/0/1` in both directions (tx+rx), we look at the interface index (`If-Index`):
```bash
$ boy -S --iface-idx 2 --memif memif0/0 --device both
```
We can verify the applied configuration:
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
Finally, we can remove the _span/mirror_ configuration:
```bash
$ boy -u 2
[INFO] boy starting
[INFO] removed 1 SPAN entry for source if-index=2 (TenGigabitEthernet1/0/1)
```

## beastie
This tool captures packets arriving on a `memif` interface and stores them in a `PCAP` file. It is normally used after creating a `memif` with `boy` and enabling a _span/mirror_ session to that interface inside VPP. `beastie` connects to the Unix domain socket specified with `--socket` or, if none is provided, to `/run/vpp/memif.sock`. Inside that socket it selects the `memif` interface by the given `id`. By default, it connects as the `slave` endpoint to the `memif` with `id=0` on `/run/vpp/memif.sock` and writes the capture to `capture.pcap`.
```bash
$ beastie
[INFO] beastie starting
[INFO] capture output: capture.pcap
[INFO] waiting for packets on memif...
```
If we want to save the capture to a different file:
```bash
$ beastie --write trace.pcap
```
We can limit the capture to a specific size, for example 1 MiB:
```bash
$ beastie --write trace.pcap --max-bytes 1048576
```
If the VPP `memif` was created with a different identifier, it must be specified with `--id`:
```bash
$ beastie --id 1 --write trace.pcap
```
If VPP uses a different socket than the default one, it can also be specified explicitly:
```bash
$ beastie --socket /run/vpp/custom.sock --write trace.pcap
```
A typical workflow looks like this:
```bash
$ boy --create-memif --id 1 --socket-id 0
$ boy -S --iface-idx 2 --memif memif0/1 --device both
$ beastie --id 1 --write trace.pcap
```
Pressing `Ctrl+C` starts the capture shutdown process; `beastie` closes the `PCAP` file cleanly and prints a summary with the captured bytes and packets.

# Appendix A: Installing VPP
We will build the stable VPP 26.02 release and create a Debian package from it. We assume that neither VPP nor DPDK is installed on the system.

First, install the minimum packages required to build it. All commands are implicitly run as _root_:
```bash
$ apt update
$ apt install -y git make sudo cmake build-essential
```
Clone the source code from the official repository:
```bash
$ git clone https://gerrit.fd.io/r/vpp
$ cd vpp
$ git checkout v26.02
```
Install the build dependencies, build VPP, and package it:
```bash
$ cd /root/vpp
$ make install-dep
$ make build-release
$ make pkg-deb
```
Since _beastie_ requires the `libmemif.so` library, we build it as well:
```bash
$ cd /root/vpp/extras/libmemif
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
$ make -j"$(nproc)"
```
Size the _hugepages_ according to the hardware. See appendices B and C for information about the APU3 and ASUSTeK P10S-I cases. Install all packages we just generated. This ensures that, in particular, the `libmemif.so` library is available:
```bash
$ cd /root/vpp/build-root
$ dpkg -i ./*.deb
```
If `dpkg` reports missing dependencies, we can force their installation:
```bash
$ apt -f install
```
Install the `libmemif.so` library:
```bash
$ cd /root/vpp/extras/libmemif/build
$ make install
```
Configure the general VPP parameters in /etc/vpp/startup.conf. It is essential to add the `memif_plugin.so` plugin.

# Appendix B: APU3 Development Platform with Debian 13
We have a PC Engines APU3D4 with 3 i211AT LAN ports, an AMD GX-412TC CPU, and 4 GB of DRAM. These network cards are compatible with DPDK. To install Debian 13, download a `netinst.iso` image from https://www.debian.org/CD/netinst/ and copy it to a USB drive:

```
$ sudo dd if=debian-13.4.0-amd64-netinst.iso of=/dev/<sdX> bs=8M status=progress
$ sudo sync
```
Make sure `/dev/sdX` is the device that is not mounted, something like `/dev/sdb` or `/dev/sdc`.
Use `picocom` to get a clean and colored serial console:
```
$ picocom -b 115200 /dev/ttyUSB0
To send a TAB character, send it in hexadecimal: C-a C-w ==> 09
To exit `picocom`: C-a C-x
```
Press F10 to access the boot menu. Boot from the inserted USB drive. Then press TAB to modify the boot parameters:
```
-/install.amd/vmlinuz vga=788 initrd=/install.amd/gtk/initrd.gz --- quiet
+/install.amd/vmlinuz vga=off initrd=/install.amd/gtk/initrd.gz --- quiet console=ttyS0,115200u8
```
Follow the usual Debian installation procedure.

## BIOS Update and Enabling IOMMU on APU3
Since we want to use VPP with the `vfio-pci` module and that requires IOMMU, APU3 must support it at the BIOS level. We therefore consider updating the BIOS to a version that supports it. We assume Linux/Debian is already installed and perform the update from that operating system.

First install `flashrom`:

```bash
$ apt update
$ apt install flashrom
```
`flashrom` must have write permissions to `/dev/mem`. Modern _kernels_ usually block access to this device even for the _root_ user. To allow it, edit `/etc/default/grub`. Under `GRUB_CMDLINE_LINUX_DEFAULT`, include the `iomem=relaxed` option. Then update the GRUB configuration with `update-grub`.

Now download the new ROM binary. IOMMU support is included starting with version v4.11.0.2:
```bash
$ wget https://3mdeb.com/open-source-firmware/pcengines/apu3/apu3_v4.19.0.1.rom
```
Now we can update it. The `flashrom` options are important:
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
Once the BIOS is updated, enable the IOMMU feature. Access the APU3 BIOS menu through the serial port and enable the IOMMU option:
```
2 Payload [setup] > Option v > Option s
```
If everything goes well, the system should show these kernel messages:
```bash
$ sudo dmesg | grep IOMMU
[    0.743352] pci 0000:00:00.2: AMD-Vi: IOMMU performance counters supported
[    0.751367] pci 0000:00:00.2: AMD-Vi: Found IOMMU cap 0x40
[   22.300235] perf/amd_iommu: Detected AMD IOMMU #0 (2 banks, 4 counters/bank).
[   22.354872] AMD-Vi: AMD IOMMUv2 loaded and initialized
```
If VPP is installed correctly, we should see it detect the network interfaces with the `vfio-pci` driver:
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
## _Hugepages_ sizing
We propose the following `etc/sysctl.d/80-vpp.conf` file:
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
and for `etc/sysctl.d/81-vpp-netlink.conf`:
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
Apply these files:
```bash
$ sysctl -p -f /etc/sysctl.d/80-vpp.conf
$ sysctl -p -f /etc/sysctl.d/81-vpp-netlink.conf
```
If we want to apply all system sysctl files and verify possible conflicts:
```bash
$ sysctl --system
```

# Appendix C: ASUSTeK P10S-I Configuration with Debian 13
We have another hardware platform. It is a server with an ASUSTeK P10S-I Series motherboard, American Megatrends Inc. BIOS version 4602, 32 GB of RAM, and an Intel(R) Xeon(R) CPU E3-1220 v5 @ 3.00GHz processor. This server is equipped with two integrated Intel I210 network cards and one PCIe Intel X710 card with four SFP+ ports.

To optimize this hardware for use with VPP, we modify the following BIOS parameters (Advanced):

| CATEGORY | BIOS OPTION | SETTING |
| ----- | ----- | ----- |
| Virtualization and PCIe Bus | Intel Virtualization Technology (VT-x) | [Enabled] |
| Virtualization and PCIe Bus | PCI Latency Timer | [32 PCI clock] |
| Power and Frequency Management | C-States (CPU Power Management) | [Disabled] |
| Power and Frequency Management | Intel SpeedStep | [Disabled] |
| Power and Frequency Management | Intel Speed Shift Technology | [Disabled] |
| Power and Frequency Management | Turbo Mode (Turbo Boost) | [Disabled] |
| Power and Frequency Management | DMI Link ASPM Control | [Disabled] |
| Processor Performance | Hardware Prefetcher | [Enabled] |
| Processor Performance | Adjacent Cache Line Prefetch | [Enabled] |
| Processor Performance | CPU AES (AES-NI) | [Enabled] |
| Security Features | SW Guard Extensions (Intel SGX) | [Disabled] |
| Security Features | Intel TXT Support | [Disabled] |

We proceed with a Debian 13 installation similar to the APU3 one. In this case, no serial port access is required.

Add an entry to `/etc/default/grub`:
```
GRUB_CMDLINE_LINUX_DEFAULT="quiet intel_iommu=on iommu=pt isolcpus=1-3 nohz_full=1-3 rcu_nocbs=1-3 processor.max_cstate=1 intel_idle.max_cstate=1"
```
This does the following:
* `intel_iommu=on iommu=pt`: Enables IOMMU (VT-d) and puts it in pass-through mode (`pt`). This gives the `vfio_pci` module direct and exclusive access to the network card.
* `isolcpus=1-3`: Tells the OS not to schedule normal tasks on cores 1, 2, and 3. In `/etc/vpp/startup.conf` we will tell VPP to use cores 2 and 3 for packet processing and core 1 for the remaining tasks.
* `nohz_full=1-3 i rcu_nocbs=1-3`: Removes clock and maintenance interrupts from those cores.
* `processor.max_cstate=1 intel_idle.max_cstate=1`: Adds an extra safety layer to prevent Linux from trying to put the cores to sleep, complementing what we already configured in the BIOS.

Apply the changes:
```bash
$ update-grub
```
We will modify the `vpp` service so that it loads the `vfio_pci` module instead of `uio_pci_generic`. Debian 13 provides a new method to override services so updates do not overwrite them. Run:
```bash
$ systemctl edit vpp.service
```
An editor will open with the current file contents commented out. We need to modify a specific section of the file. Add these lines to remove the previous `ExecStartPre` entry and replace its value:
```
[Service]
ExecStartPre=
ExecStartPre=-/sbin/modprobe vfio_pci
```
Since the configuration we will use includes a log file at `/var/log/vpp/vpp.log`, create it because the default VPP installation does not create it. Apply the changes and restart VPP:
```bash
$ systemctl daemon-reload
$ systemctl restart vpp
```
If we inspect the service log, after the plugin loading phase we should see something similar to this:
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
One key line is `Using IOMMU type 1 (Type 1)`. This confirms that the Debian kernel recognized the BIOS configuration (VT-d) and that the `vfio_pci` module has full control over the hardware. `Type 1` is the safest and most efficient mode for PCIe pass-through on Linux. Another key line is `Using Vector AVX2 Scattered`. It appears thanks to enabling _Hardware Prefetcher_ and _Adjacent Cache Line Prefetch_ in the BIOS. `Vector AVX2` means VPP is using SIMD (_Single Instruction Multiple Data_) instructions to process packets in batches. `Scattered` indicates that the DPDK driver (`i40e` for Intel X710 cards) can efficiently handle packets fragmented in memory.

## _Hugepages_ sizing
We propose the following `etc/sysctl.d/80-vpp.conf` file:
```
# VPP on medium/large systems (32 GB RAM, 4 Cores)
# Generous values for high throughput and stable memory allocation.

# Reserve hugepages for DPDK/VPP (2 MB pages)
# 4096 * 2 MB = 8 GB reserved for hugepages. 
# (Increased to support more traffic and large BGP tables without starving 32 GB of RAM)
vm.nr_hugepages = 4096

# Allow large shared-memory and hugepage-backed mappings
# Doubled to avoid limits in environments with many interfaces or containers
vm.max_map_count = 524288

# Keep swapping low; VPP/DPDK hate latency spikes
vm.swappiness = 10

# Leave some room before the kernel starts reclaim pressure
# 512 MB of free-memory cushion to avoid allocation stalls in the kernel (was 128 MB)
vm.min_free_kbytes = 524288

# Permit enough shared memory for VPP segments, memif, stats, etc.
# Allow up to 16 GB of shared memory (roughly half of RAM)
kernel.shmmax = 17179869184
kernel.shmall = 4194304

# Larger socket buffers help control-plane/API tools and capture paths
# Increase the maximums to 64 MB to support large bursts
net.core.rmem_default = 524288
net.core.rmem_max = 67108864
net.core.wmem_default = 524288
net.core.wmem_max = 67108864

# Backlog for bursts
# Much more room for bursts, suitable for a machine that can process traffic quickly (4 cores)
net.core.netdev_max_backlog = 65536

# More room for local sockets used by agents/tools around VPP
net.unix.max_dgram_qlen = 2048
```
and for `etc/sysctl.d/81-vpp-netlink.conf`:
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
Apply these files:
```bash
$ sysctl -p -f /etc/sysctl.d/80-vpp.conf
$ sysctl -p -f /etc/sysctl.d/81-vpp-netlink.conf
```
If we want to apply all system sysctl files and verify possible conflicts:
```bash
$ sysctl --system
```
