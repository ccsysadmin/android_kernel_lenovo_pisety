Lenovo P70-A Kernel Source
==============

Basic   | Spec Sheet
-------:|:-------------------------
CPU     | Octa-core 1.7 GHz Cortex-A53
GPU     | Mali-T760MP2
Memory  | 2GB RAM
Storage | 8GB
Display | 5.0" IPS 1280 x 720 px
Camera  | Primary 13 MP, f/2.0, autofocus, LED flash, Secondary 5 MP

* Compilation
        
        $ export CROSS_COMPILE=aarch64-linux-android-

        $ export PATH=~/toolchains/aarch64-linux-android-4.9/bin:$PATH

        $ export ARCH=arm64

        $ make P70_defconfig ARCH=arm64 CROSS_COMPILE=aarch64-linux-android-

        $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-android-
