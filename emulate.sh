#!/bin/bash

QEMU_STM32=../qemu_stm32/arm-softmmu/qemu-system-arm

CUR=`dirname $0`

emulate () {
	$QEMU_STM32 \
		-M stm32-p103 \
		-kernel $1 \
		-parallel none \
		-monitor tcp:localhost:4444,server,nowait <&0 & pid=$!
}

stm32_qemu () {
	emulate $1

	echo "Modeling STM32 in QEMU..."
	if ! wait $pid; then
		echo
		echo "Modeling failed to execute in $2 seconds, giving up."
		exit -1
	fi
}

stm32_qemu $1 5
