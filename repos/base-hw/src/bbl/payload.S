.section ".payload","a",@progbits
.align 3

.globl _payload_start, _payload_end
_payload_start:
.incbin BBL_PAYLOAD
_payload_end:
