#pragma once

#define ROP_THUMBCODE(addr, inst_skips) (((addr) |  1) + (inst_skips) * 2)
#define ROP_ARMCODE(addr, inst_skips)   (((addr) & ~3) + (inst_skips) * 4)

// some macro define versions of ipc inlines

#define IPC_BUFFER_R  (1<<1)
#define IPC_BUFFER_W  (1<<2)
#define IPC_BUFFER_RW (IPC_BUFFER_R | IPC_BUFFER_W)

#define IPC_MakeHeader(command_id, normal_params, translate_params) ((command_id & 0xFFFF) << 16) | ((normal_params & 0x3F) << 6) | ((translate_params & 0x3F) << 0)

#define IPC_Desc_SharedHandles(number) ((number - 1) << 26)

#define IPC_Desc_StaticBuffer(size, buffer_id) (((size) << 14) | (((buffer_id) & 0xF) << 10) | 0x2)

#define IPC_Desc_Buffer(size, rights) (((size) << 4) | 0x8 | ((rights) & 0x7))
