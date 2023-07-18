# nimdpshax

An altered version of nimhax to pwn dsp with an emulated ps:ps service, set GPUPROT and exploit kernel.

# Required sysmodule versions

- httpwn is fixed up for HTTP `v14336` (introduced in 11.4)
- nimhax is prepared for NIM `v14341` (introduced in 11.8)
- dsp pwn is prepared for DSP `v7169` (introduced in 11.1)

Any version between since 11.8 until and including 11.17 (latest as of this writing) should be able to run it fine.

# Credits

(Included credits from original [ctr-httpwn](https://github.com/yellows8/ctr-httpwn))

* @Tuxsh for the [exploit writeup](https://gist.github.com/TuxSH/c7a236ea59f363314e93daa60fefd983) in C for killing PS, emulating it and ipctakeover DSP and set GPUPROT to 0.
* @Tuxsh for the [>= 11.4 httpwn method](https://github.com/TuxSH/universal-otherapp/blob/ae4c6cea93c571ce7e792f9ab7d0ef97224bf2cf/source/httpwn.c)
* @zoogie for fix up on the httpwn sharedmem rop generation code and addresses with the mentioned above resourse. As well fix up bosshaxx addresses.
* This uses the decompression code from here for ExeFS .code decompression: https://github.com/smealum/ninjhax2.x/blob/master/app_bootloader/source/takeover.c
* Tinyxml2 is used for config XML parsing, via portlibs.
* @ihaveamac for the app icon(issue #1).
* types.h at ipctakeover/boss/ is from ctrtool.
* The filepath for "url_config.txt" is from here: https://github.com/skiptirengu/ctr-httpwn

# Side note

This is a duplicate repository of [ctr-httpwn](https://github.com/yellows8/ctr-httpwn) because this did not start as a fork of it, this was initialized differently and also could not private fork the repository. This started as a private repository.
