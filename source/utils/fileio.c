#include <3ds.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <utils/fileio.h>

/**
 * For some reason, I don't know why, I don't know how,
 * occasionally and in certain scenarios, fopen fails on
 * SDMC files.
 * Either ctrulib or newlib init tbh. Likely ctrulib.
 * I don't feel like trying to debug an OFW *hax2 launched
 * homebrew environment, and after an XML takeover.
 * I will instead is provide abstraction to file opening,
 * reading, writing, etc, by using stdio, when possible,
 * and use FSUSER calls directly for SDMC if those fail.
 * Other mount points, like romfs, will not be opened with
 * the alternative workaround code, we'll just use the
 * standard io.
 * That would mean more path handling that I wish for my
 * own sanity.
 * I'll make it libc-like arguments.
 * YET, I still did more than I actually needed.
 * I needed just open, close, read and write.
 * And also, I know that close code is not fail proof!
 * If you use this in other purposes, note that this is
 * not fully thread safe on close!
 * Also seek doesn't behave fully to standard.
 * But hey, I'm lazy. Kinda.
 */

static const FS_Path sdmcPath = {PATH_EMPTY, 1, ""};

typedef struct {
	void* (*open)(const char* pathname, const char* mode);
	int (*close)(void* stream);
	size_t (*read)(void* ptr, size_t size, size_t nmemb, void* stream);
	size_t (*write)(void* ptr, size_t size, size_t nmemb, void* stream);
	int (*seek)(void* stream, long offset, int whence);
	long (*tell)(void* stream);
	void (*rewind)(void* stream);
	int (*flush)(void* stream);
} _io_handler;

struct __FILEIO {
	const _io_handler* handler;
	void* _data;
};

#define DFSS_MAGIC (0x64465353)
#define BAD_MAGIC (0xDEADBEEF)

struct __directFSFileStream {
	u32 magic; // dFSS - 0x64465353
	LightLock lock;
	Handle file;
	u32 fs_flags;
	u64 offset;
};

static int parse_mode_to_flags(const char* mode, u32* out_fs_flags, bool* truncate, bool* append) {
	u32 fs_flags = 0;
	bool do_truncate = false;
	bool do_append = false;
	const char* ptr = NULL;

	if        (mode[0] == 'r') {
		fs_flags |= FS_OPEN_READ;
		if (mode[1] == '+') {
			fs_flags |= FS_OPEN_WRITE;
			ptr = &mode[2];
		} else ptr = &mode[1];
	}

	else if (mode[0] == 'w') {
		fs_flags |= FS_OPEN_WRITE | FS_OPEN_CREATE;
		if (mode[1] == '+') {
			fs_flags |= FS_OPEN_READ;
			ptr = &mode[2];
		} else ptr = &mode[1];
		do_truncate = true;
	}

	else if (mode[0] == 'a') {
		fs_flags |= FS_OPEN_WRITE | FS_OPEN_CREATE;
		if (mode[1] == '+') {
			fs_flags |= FS_OPEN_READ;
			ptr = &mode[2];
		} else ptr = &mode[1];
		do_append = true;
	}

	else return -1;

	if (*ptr != 'b') return -1;

	*out_fs_flags = fs_flags;
	*truncate = do_truncate;
	*append = do_append;
	return 0;
}

// still wont sanitize like .. and .
static u16* sanatize_paths(const char* cur_dir, const char* pathname, int* out_size) {
	u8 (*utf8_paths)[2][PATH_MAX * 4 + 1];
	u16 (*utf16_paths)[2][PATH_MAX * 2 + 1];

	utf8_paths = (u8 (*)[2][PATH_MAX * 4 + 1])calloc(1, sizeof(*utf8_paths));
	utf16_paths = (u16 (*)[2][PATH_MAX * 2 + 1])calloc(1, sizeof(*utf16_paths));

	if (!utf8_paths || !utf16_paths) {
		free(utf8_paths);
		free(utf16_paths);
		return NULL;
	}

	int cur_dir_len = 0;
	int pathname_len = 0;

	for (; *cur_dir && cur_dir_len < PATH_MAX * 4; ++cur_dir_len) {
		char c = *cur_dir;
		(*utf8_paths)[0][cur_dir_len] = c;
		++cur_dir;
		if (c == '/') {
			while (*cur_dir == '/') ++cur_dir;
		}
	}

	for (; *pathname && pathname_len < PATH_MAX * 4; ++pathname_len) {
		char c = *pathname;
		(*utf8_paths)[1][pathname_len] = c;
		++pathname;
		if (c == '/') {
			while (*pathname == '/') ++pathname;
		}
	}

	if (!cur_dir_len || !pathname_len || (pathname_len == 1 && (*utf8_paths)[0][0] == '/')) {
		free(utf8_paths);
		free(utf16_paths);
		return NULL;
	}

	if ((*utf8_paths)[0][cur_dir_len-1] == '/') {
		(*utf8_paths)[0][--cur_dir_len] = 0;
	}

	if ((*utf8_paths)[1][pathname_len-1] == '/') {
		(*utf8_paths)[1][--pathname_len] = 0;
	}

	if (utf8_to_utf16((*utf16_paths)[0], (*utf8_paths)[0], PATH_MAX * 2) < 0 ||
	  utf8_to_utf16((*utf16_paths)[1], (*utf8_paths)[1], PATH_MAX * 2) < 0) {
		free(utf8_paths);
		free(utf16_paths);
		return NULL;
	}

	free(utf8_paths);

	for (cur_dir_len = 0; (*utf16_paths)[0][cur_dir_len]; ++cur_dir_len) {};
	for (pathname_len = 0; (*utf16_paths)[1][pathname_len]; ++pathname_len) {};

	bool cur_dir_need_slash = (*utf16_paths)[0][0] != '/' && cur_dir_len > 1; // if originally just an "/", ignore adding
	bool pathname_need_slash = (*utf16_paths)[1][0] != '/';

	int total_size = cur_dir_len + pathname_len + cur_dir_need_slash + pathname_need_slash;
	if (total_size > PATH_MAX) {
		free(utf16_paths);
		return NULL;
	}

	u16* final_path = calloc(sizeof(u16), total_size + 1);
	u16* writting_ptr = final_path;
	if (cur_dir_need_slash) {
		*writting_ptr = '/';
		++writting_ptr;
	}
	memcpy(writting_ptr, (*utf16_paths)[0], cur_dir_len * sizeof(u16));
	writting_ptr += cur_dir_len;
	if (pathname_need_slash) {
		*writting_ptr = '/';
		++writting_ptr;
	}
	memcpy(writting_ptr, (*utf16_paths)[1], pathname_len * sizeof(u16));

	free(utf16_paths);
	*out_size = (total_size+1)*sizeof(u16);
	return final_path;
}

static FS_Path sdmc_process_path(const char* pathname) {
	FS_Path fs_path = {PATH_UTF16, 0, NULL};

	do {
		const char* semicolon = strchr(pathname, ':');
		if (!semicolon) break;
		if (strncmp(pathname, "sdmc:", 5)) return fs_path;
		pathname = semicolon + 1;
	} while(0);

	if (pathname[0] == 0)
		return fs_path;

	if (pathname[0] == '/') {
		int path_size = 0;
		u16* path = sanatize_paths("/", pathname, &path_size);

		fs_path.size = path_size;
		fs_path.data = path;
		return fs_path;
	}

	char* cwd = (char*)malloc(PATH_MAX+9);
	if (!cwd || !getcwd(cwd, PATH_MAX+9) || (cwd[0] != '/' && strncmp(cwd, "sdmc:/", 6))) {
		free(cwd);
		return fs_path;
	}

	int path_size = 0;
	u16* path = sanatize_paths(cwd + (cwd[0] == '/' ? 0 : 5), pathname, &path_size);
	free(cwd);

	fs_path.size = path_size;
	fs_path.data = path;
	return fs_path;
}

static void* sdmc_open(const char* pathname, const char* mode) {
	u32 flags;
	bool truncate;
	bool append;
	if (parse_mode_to_flags(mode, &flags, &truncate, &append))
		return NULL;

	FS_Path clear_path = sdmc_process_path(pathname);
	if (clear_path.size == 0 || clear_path.data == NULL) {
		free((void*)clear_path.data);
		return NULL;
	}

	// just debug print
	#if 0
	printf("Opening: ");
	for (uint16_t* _path = (uint16_t*)clear_path.data; (u32)_path < (u32)clear_path.data + clear_path.size; ) {
		uint8_t utf8[5] = {0};
		uint32_t utf32;
		ssize_t step = decode_utf16(&utf32, _path);
		if (step < 0) break;
		_path += step;
		step = encode_utf8(utf8, utf32);
		if (step < 0) break;
		printf("%s", (char*)utf8);
	}
	puts("\n");
	#endif

	Handle filehandle;
	Result res = FSUSER_OpenFileDirectly(&filehandle, ARCHIVE_SDMC, sdmcPath, clear_path, flags, 0);

	free((void*)clear_path.data);

	if (R_FAILED(res))
		return NULL;

	if (truncate) {
		res = FSFILE_SetSize(filehandle, 0);
		if (R_FAILED(res)) {
			FSFILE_Close(filehandle);
			svcCloseHandle(filehandle);
			return NULL;
		}
	}

	u64 offset = 0;

	if (append) {
		res = FSFILE_GetSize(filehandle, &offset);
		if (R_FAILED(res)) {
			FSFILE_Close(filehandle);
			svcCloseHandle(filehandle);
			return NULL;
		}
	}

	struct __directFSFileStream* stream = (struct __directFSFileStream*)malloc(sizeof(struct __directFSFileStream));
	if (!stream) {
		FSFILE_Close(filehandle);
		svcCloseHandle(filehandle);
		return NULL;
	}

	stream->magic = DFSS_MAGIC;
	LightLock_Init(&stream->lock);
	stream->file = filehandle;
	stream->fs_flags = flags;
	stream->offset = offset;

	return stream;
}

static int sdmc_close(void* stream) {
	struct __directFSFileStream* _stream = (struct __directFSFileStream*)stream;

	LightLock_Lock(&_stream->lock);

	if (!_stream || _stream->magic != DFSS_MAGIC) {
		LightLock_Unlock(&_stream->lock);
		return EOF;
	}

	Result res = FSFILE_Close(_stream->file);
	svcCloseHandle(_stream->file);

	_stream->magic = BAD_MAGIC;

	LightLock_Unlock(&_stream->lock);

	free(stream);
	return R_FAILED(res) ? EOF : 0;
}

static size_t sdmc_read(void *ptr, size_t size, size_t nmemb, void* stream) {
	u64 total_size = size * nmemb;
	if (total_size > (BIT(31)-1)) {
		return 0;
	}

	struct __directFSFileStream* _stream = (struct __directFSFileStream*)stream;

	LightLock_Lock(&_stream->lock);
	if (!_stream || _stream->magic != DFSS_MAGIC || !(_stream->fs_flags & FS_OPEN_READ)) {
		LightLock_Unlock(&_stream->lock);
		return 0;
	}
	u32 read;
	Result res = FSFILE_Read(_stream->file, &read, _stream->offset, ptr, (u32)total_size);
	if (!R_FAILED(res)) {
		_stream->offset += read;
	} else read = 0;
	LightLock_Unlock(&_stream->lock);

	return read / size;
}

static size_t sdmc_write(void* ptr, size_t size, size_t nmemb, void* stream) {
	u64 total_size = size * nmemb;
	if (total_size > (BIT(31)-1)) {
		return 0;
	}

	struct __directFSFileStream* _stream = (struct __directFSFileStream*)stream;

	LightLock_Lock(&_stream->lock);

	if (!_stream || _stream->magic != DFSS_MAGIC || !(_stream->fs_flags & FS_OPEN_WRITE)) {
		LightLock_Unlock(&_stream->lock);
		return 0;
	}
	u32 written;
	Result res = FSFILE_Write(_stream->file, &written, _stream->offset, ptr, (u32)total_size, FS_WRITE_FLUSH);
	if (!R_FAILED(res)) {
		_stream->offset += written;
	} else written = 0;
	LightLock_Unlock(&_stream->lock);

	return written / size;
}

static int sdmc_seek(void* stream, long offset, int whence) {
	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
		return -1;
	}

	struct __directFSFileStream* _stream = (struct __directFSFileStream*)stream;

	LightLock_Lock(&_stream->lock);

	if (!_stream || _stream->magic != DFSS_MAGIC) {
		LightLock_Unlock(&_stream->lock);
		return -1;
	}

	if (offset == 0) {
		LightLock_Unlock(&_stream->lock);
		return 0;
	}

	u64 size;
	Result res = FSFILE_GetSize(_stream->file, &size);
	if (R_FAILED(res)) {
		LightLock_Unlock(&_stream->lock);
		return -1;
	}

	if (whence == SEEK_SET) {
		if (offset < 0) {
			LightLock_Unlock(&_stream->lock);
			return -1;
		}
		if ((u64)offset > size) {
			_stream->offset = size;
		} else _stream->offset = offset;
	} else if (whence == SEEK_CUR) {
		if (offset < 0) {
			u64 _offset = (u64)(-(s64)offset);
			if (_offset > _stream->offset) {
				LightLock_Unlock(&_stream->lock);
				return -1;
			}
			_stream->offset -= _offset;
		} else {
			u64 _offset = _stream->offset + offset;
			if (_offset < _stream->offset || _offset < (u64)offset) { // overflow check
				LightLock_Unlock(&_stream->lock);
				return -1;
			}
			if (_offset > size)
				_stream->offset = size;
			else _stream->offset = _offset;
		}
	} else if (whence == SEEK_END) {
		if (offset >= 0) {
			_stream->offset = size;
		} else {
			if ((u64)(-(s64)offset) > size) {
				LightLock_Unlock(&_stream->lock);
				return -1;
			}
			_stream->offset = size + offset; // offset is negative 
		}
	}

	LightLock_Unlock(&_stream->lock);
	return 0;
}

static long sdmc_tell(void* stream) {
	struct __directFSFileStream* _stream = (struct __directFSFileStream*)stream;

	LightLock_Lock(&_stream->lock);

	if (!_stream || _stream->magic != DFSS_MAGIC) {
		LightLock_Unlock(&_stream->lock);
		return -1;
	}

	u64 size;
	Result res = FSFILE_GetSize(_stream->file, &size);
	if (R_FAILED(res)) {
		size = -1;
	}

	LightLock_Unlock(&_stream->lock);
	return (long)size;
}

static void sdmc_rewind(void* stream) {
	struct __directFSFileStream* _stream = (struct __directFSFileStream*)stream;

	LightLock_Lock(&_stream->lock);

	if (!_stream || _stream->magic != DFSS_MAGIC) {
		LightLock_Unlock(&_stream->lock);
		return;
	}

	_stream->offset = 0;

	LightLock_Unlock(&_stream->lock);
}

static int sdmc_flush(void* stream) {
	(void)stream;
	return 0; // stub
}

const _io_handler standard_io = {
	.open   = (void* (*)(const char*, const char*))&fopen,
	.close  = (int (*)(void*))&fclose,
	.read   = (size_t (*)(void*, size_t, size_t, void*))&fread,
	.write  = (size_t (*)(void*, size_t, size_t, void*))&fwrite,
	.seek   = (int (*)(void*, long, int))&fseek,
	.tell   = (long (*)(void*))&ftell,
	.rewind = (void (*)(void*))&rewind,
	.flush  = (int (*)(void*))&fflush
};

const _io_handler sdmc_workaround = {
	.open   = &sdmc_open,
	.close  = &sdmc_close,
	.read   = &sdmc_read,
	.write  = &sdmc_write,
	.seek   = &sdmc_seek,
	.tell   = &sdmc_tell,
	.rewind = &sdmc_rewind,
	.flush  = &sdmc_flush
};

static FILEIO* fileio_tryopen(const _io_handler* handler, const char* pathname, const char* mode) {
	FILEIO* io = (FILEIO*)malloc(sizeof(FILEIO));
	if (!io) {
		return NULL;
	}

	void* _data = handler->open(pathname, mode);
	if (!_data) {
		free(io);
		return NULL;
	}

	io->_data = _data;
	io->handler = handler;
	return io;
}

FILEIO* fileio_open(const char* pathname, const char* mode) {
	FILEIO* io;
	io = fileio_tryopen(&standard_io, pathname, mode);
	if (io) return io;
	io = fileio_tryopen(&sdmc_workaround, pathname, mode);
	return io;
}

int fileio_close(FILEIO* stream) {
	int ret = stream->handler->close(stream->_data);
	free(stream);
	return ret;
}

size_t fileio_read(void* ptr, size_t size, size_t nmemb, FILEIO* stream) {
	return stream->handler->read(ptr, size, nmemb, stream->_data);
}

size_t fileio_write(void* ptr, size_t size, size_t nmemb, FILEIO* stream) {
	return stream->handler->write(ptr, size, nmemb, stream->_data);
}

int fileio_seek(FILEIO* stream, long offset, int whence) {
	return stream->handler->seek(stream->_data, offset, whence);
}

long fileio_tell(FILEIO* stream) {
	return stream->handler->tell(stream->_data);
}

void fileio_rewind(FILEIO* stream, long offset, int whence) {
	return stream->handler->rewind(stream->_data);
}

int fileio_flush(FILEIO* stream) {
	return stream->handler->flush(stream->_data);
}
