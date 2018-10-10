content: lx_block.img

lx_block.img:
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
