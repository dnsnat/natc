#!/bin/bash
#本程序依赖onion jansson 底部有交叉编译示例

#编译当前平台客户端
if [ -z $1 ];then
    make clean
    make
fi

#编译ea4500客户端
if [ "$1" == "arm" ];then
    export LC_ALL=C
    export CC=arm-openwrt-linux-gcc
    export AR=arm-openwrt-linux-ar
    export STAGING_DIR=/opt/toolchain/openwrt_sdk/openwrt-sdk-18.06.0-kirkwood_gcc-7.3.0_musl_eabi.Linux-x86_64/staging_dir/toolchain-arm_xscale_gcc-7.3.0_musl_eabi
    
    make clean
    make
fi

if [ "$1" == "mips" ];then
    export CC=mipsel-openwrt-linux-gcc
    export AR=mipsel-openwrt-linux-ar
    export STAGING_DIR=/opt/toolchain/openwrt_sdk/openwrt-sdk-ramips-rt305x_gcc-7.3.0_musl.Linux-x86_64/staging_dir
    
    make clean
    make
fi

#以下为思科ea4500路由openwrt交叉编译环境 onion jansson库的安装示例
#onion 库安装
#export LC_ALL=C
#export CC=arm-openwrt-linux-gcc
#export AR=arm-openwrt-linux-ar
#export STAGING_DIR=/opt/toolchain/openwrt_sdk/openwrt-sdk-18.06.0-kirkwood_gcc-7.3.0_musl_eabi.Linux-x86_64/staging_dir/toolchain-arm_xscale_gcc-7.3.0_musl_eabi
#cd onion
#mkdir build && cd build
#cmake .. -DONION_USE_SYSTEMD=false -DONION_USE_SSL=false -DONION_USE_SQLITE3=false -DONION_USE_REDIS=false -DONION_USE_PAM=false -DONION_USE_XML2=false -DONION_USE_PNG=false -DONION_USE_JPEG=false -DONION_EXAMPLES=false -DONION_USE_TESTS=false  -DCMAKE_INSTALL_PREFIX=/opt/toolchain/openwrt_sdk/openwrt-sdk-18.06.0-kirkwood_gcc-7.3.0_musl_eabi.Linux-x86_64/staging_dir/toolchain-arm_xscale_gcc-7.3.0_musl_eabi/usr
#make && make install

#jansson 库安装
#cd jansson
#mkdir build && cd build
#cmake .. -DCMAKE_INSTALL_PREFIX=/opt/toolchain/openwrt_sdk/openwrt-sdk-18.06.0-kirkwood_gcc-7.3.0_musl_eabi.Linux-x86_64/staging_dir/toolchain-arm_xscale_gcc-7.3.0_musl_eabi/usr
#make && make install

