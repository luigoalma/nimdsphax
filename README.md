# nimhax

A demonstration of using httpwn to use ipctakeover on `nim:s` and get `cfg:s` and `am:net` handles, as well `fs:USER` and its permissions to dump `movable.sed` using IVS export.

An extra demonstration as well for using `nim:s`'s ropped service to run another ipctakeover on AM11.

# am11pwn

Using a fake file service with any command that takes a file handle could be used to push a rop with ipctakeover.

Code demonstrates an example with `am:net` and a rop prepared for `AM_GetCiaRequiredSpace`, custom replying with `pxi:am9` handle to us. Rop in this demonstration can also be ran on same command for am:u but needs to be fixed up by defining `AM_U_TARGET` on `am_rop.h`

AM sessions have more unpredictability of the stack positioning for the session thread, but AM has enough ROP gadgets to make a dynamic rop in the exchange for extra ROP size.

# Required sysmodule versions

- httpwn is fixed up for HTTP `v14336` (introduced in 11.4)
- nimhax is prepared for NIM `v14341` (introduced in 11.8)
- am11pwn is prepared for AM `v10245` (introduced in 11.8)

# Credits

(Included credits from original [ctr-httpwn](https://github.com/yellows8/ctr-httpwn))

* @Tuxsh for the [>= 11.4 httpwn method](https://github.com/TuxSH/universal-otherapp/blob/ae4c6cea93c571ce7e792f9ab7d0ef97224bf2cf/source/httpwn.c)
* @zoogie for fix up on the httpwn sharedmem rop generation code and addresses with the mentioned above resourse. As well fix up bosshaxx addresses.
* This uses the decompression code from here for ExeFS .code decompression: https://github.com/smealum/ninjhax2.x/blob/master/app_bootloader/source/takeover.c
* Tinyxml2 is used for config XML parsing, via portlibs.
* @ihaveamac for the app icon(issue #1).
* types.h at ipctakeover/boss/ is from ctrtool.
* The filepath for "url_config.txt" is from here: https://github.com/skiptirengu/ctr-httpwn

