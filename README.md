# DPDK-HTTPDUMP

## Config System
```
vim /etc/default/grub
    GRUB_CMDLINE_LINUX_DEFAULT="GRUB_CMDLINE_LINUX_DEFAULT="isolcpus=1-26 default_hugepagesz=1G hugepagesz=1G hugepages=32""
echo never > /sys/kernel/mm/transparent_hugepage/enabled
echo never > /sys/kernel/mm/transparent_hugepage/defrag
```
1. isolcpus
    - 将指定cpu设置为独占状态
    - 建议仅隔离所需使用的CPU核心，一般不要使用core 0
    - 格式：1，2，3 或者 1-3
2. hugepages
    - 设置hugepages大小及数量

## Config DPDK
```
wget http://fast.dpdk.org/rel/dpdk-17.05.tar.xz
tar -xf dpdk.tar.xz
./DPDK/usertools/dpdk-setup.sh
```
设置顺序：12, 15, 21

## Config DPDK-httpdump

- httpdump.c: 171

## Compile and Usage
```
make
sudo ./dpdk-httpdump
```
