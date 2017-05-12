.section ".payload","a",@progbits
.align 3

.globl _payload_start, _payload_end
_payload_start:
#ifdef BBL_PAYLOAD
.incbin BBL_PAYLOAD
#endif
_payload_end:
