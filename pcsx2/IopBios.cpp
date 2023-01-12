/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "R3000A.h"
#include "Common.h"
#include "R5900.h" // for g_GameStarted
#include "IopBios.h"
#include "IopMem.h"

#include <cstdio>
#include <ctype.h>
#include <string.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

// set this to 0 to disable rewriting 'host:' paths!
#define USE_HOST_REWRITE 1

#if USE_HOST_REWRITE
#	ifdef _WIN32
		// disable this if you DON'T want "host:/usr/local/" paths
		// to get rewritten into host:/
#		define HOST_REWRITE_USR_LOCAL 1
#	else
		// unix/linux users might want to set it to 1
		// if they DO want to keep demos from accessing their systems' /usr/local
#		define HOST_REWRITE_USR_LOCAL 0
#	endif

static char HostRoot[1024];
#endif

void Hle_SetElfPath(const char* elfFileName)
{
#if USE_HOST_REWRITE
	const char* pos1 = strrchr(elfFileName,'/');
	const char* pos2 = strrchr(elfFileName,'\\');

	if(pos2 > pos1) // we want the LAST path separator
		pos1=pos2;

	if(!pos1) // if pos1 is NULL, then pos2 was not > pos1, so it must also be NULL
	{
		// use %CD%/host/
		char* cwd = getcwd(HostRoot,1000); // save the other 23 chars to append /host/ :P
		HostRoot[1000]=0; // Be Safe.
		if (cwd == nullptr)
			return;

		char* last = HostRoot + strlen(HostRoot) - 1;

		if((*last!='/') && (*last!='\\')) // PathAppend()-ish
			last++;

		strcpy(last,"/host/");

		return;
	}

	int len = pos1-elfFileName+1;
	memcpy(HostRoot,elfFileName,len); // include the / (or \\)
	HostRoot[len] = 0;
#endif
}

namespace R3000A {

#define v0 (psxRegs.GPR.n.v0)
#define a0 (psxRegs.GPR.n.a0)
#define a1 (psxRegs.GPR.n.a1)
#define a2 (psxRegs.GPR.n.a2)
#define a3 (psxRegs.GPR.n.a3)
#define sp (psxRegs.GPR.n.sp)
#define ra (psxRegs.GPR.n.ra)
#define pc (psxRegs.pc)

#define Ra0 (iopMemReadString(a0))
#define Ra1 (iopMemReadString(a1))
#define Ra2 (iopMemReadString(a2))
#define Ra3 (iopMemReadString(a3))

static std::string host_path(const std::string path)
{
// WIP code. Works well on win32, not so sure on unixes
// TODO: get rid of dependency on CWD/PWD
#if USE_HOST_REWRITE
	// we want filenames to be relative to pcs2dir / host

	std::string pathMod;

	// partial "rooting",
	// it will NOT avoid a path like "../../x" from escaping the pcsx2 folder!


	const std::string _local_root = "/usr/local/";
	if (HOST_REWRITE_USR_LOCAL && 0 == path.compare(0, _local_root.size(), _local_root.data())) {
		return HostRoot + path.substr(_local_root.size());
	} else if ((path[0] == '/') || (path[0] == '\\') || (isalpha(path[0]) && (path[1] == ':'))) // absolute NATIVE path (X:\blah)
	{
		// TODO: allow some way to use native paths in non-windows platforms
		// maybe hack it so linux prefixes the path with "X:"? ;P
		// or have all platforms use a common prefix for native paths
		// FIXME: Why the hell would we allow this?
		return path;
	} else // relative paths
		return HostRoot + path;

	return pathMod;
#else
	return path;
#endif
}

// TODO: sandbox option, other permissions
class HostFile : public IOManFile
{
public:
	int fd;

	HostFile(int hostfd)
	{
		fd = hostfd;
	}

	virtual ~HostFile() = default;

	static __fi int translate_error(int err)
	{
		if (err >= 0)
			return err;

		switch(err)
		{
			case -ENOENT:
				return -IOP_ENOENT;
			case -EACCES:
				return -IOP_EACCES;
			case -EISDIR:
				return -IOP_EISDIR;
			case -EIO:
			default:
				return -IOP_EIO;
		}
	}

	static int open(IOManFile **file, const std::string &full_path, s32 flags, u16 mode)
	{
		const std::string path = full_path.substr(full_path.find(':') + 1);

		// host: actually DOES let you write!
		//if (flags != IOP_O_RDONLY)
		//	return -IOP_EROFS;

		int native_flags = O_BINARY; // necessary in Windows.

		switch(flags&IOP_O_RDWR)
		{
		case IOP_O_RDONLY:	native_flags |= O_RDONLY;	break;
		case IOP_O_WRONLY:	native_flags |= O_WRONLY; break;
		case IOP_O_RDWR:	native_flags |= O_RDWR;	break;
		}

		if(flags&IOP_O_APPEND)	native_flags |= O_APPEND;
		if(flags&IOP_O_CREAT)	native_flags |= O_CREAT;
		if(flags&IOP_O_TRUNC)	native_flags |= O_TRUNC;

		int hostfd = ::open(host_path(path).data(), native_flags);
		if (hostfd < 0)
			return translate_error(hostfd);

		*file = new HostFile(hostfd);
		if (!*file)
			return -IOP_ENOMEM;

		return 0;
	}

	virtual void close()
	{
		::close(fd);
		delete this;
	}

	virtual int lseek(s32 offset, s32 whence)
	{
		int err;

		switch (whence)
		{
			case IOP_SEEK_SET:
				err = ::lseek(fd, offset, SEEK_SET);
				break;
			case IOP_SEEK_CUR:
				err = ::lseek(fd, offset, SEEK_CUR);
				break;
			case IOP_SEEK_END:
				err = ::lseek(fd, offset, SEEK_END);
				break;
			default:
				return -IOP_EIO;
		}

		return translate_error(err);
	}

	virtual int read(void *buf, u32 count)
	{
		return translate_error(::read(fd, buf, count));
	}

	virtual int write(void *buf, u32 count)
	{
		return translate_error(::write(fd, buf, count));
	}
};

namespace ioman {
	const int firstfd = 0x100;
	const int maxfds = 0x100;
	int openfds = 0;

	int freefdcount()
	{
		return maxfds - openfds;
	}

	struct filedesc
	{
		enum {
			FILE_FREE,
			FILE_FILE,
			FILE_DIR,
		} type;
		union {
			IOManFile *file;
			IOManDir *dir;
		};

		constexpr filedesc(): type(FILE_FREE), file(nullptr) {}
		operator bool() const { return type != FILE_FREE; }
		operator IOManFile*() const { return type == FILE_FILE ? file : NULL; }
		operator IOManDir*() const { return type == FILE_DIR ? dir : NULL; }
		void operator=(IOManFile *f) { type = FILE_FILE; file = f; openfds++; }
		void operator=(IOManDir *d) { type = FILE_DIR; dir = d; openfds++; }

		void close()
		{
			switch (type)
			{
				case FILE_FILE:
					file->close();
					file = NULL;
					break;
				case FILE_DIR:
					dir->close();
					dir = NULL;
					break;
				case FILE_FREE:
					return;
			}

			type = FILE_FREE;
			openfds--;
		}
	};

	filedesc fds[maxfds];

	template<typename T>
	T* getfd(int fd)
	{
		fd -= firstfd;

		if (fd < 0 || fd >= maxfds)
			return NULL;

		return fds[fd];
	}

	template <typename T>
	int allocfd(T *obj)
	{
		for (int i = 0; i < maxfds; i++)
		{
			if (!fds[i])
			{
				fds[i] = obj;
				return firstfd + i;
			}
		}

		obj->close();
		return -IOP_EMFILE;
	}

	void freefd(int fd)
	{
		fd -= firstfd;

		if (fd < 0 || fd >= maxfds)
			return;

		fds[fd].close();
	}

	void reset()
	{
		for (int i = 0; i < maxfds; i++)
		{
			if (fds[i])
				fds[i].close();
		}
	}

	bool is_host(const std::string path)
	{
		auto not_number_pos = path.find_first_not_of("0123456789", 4);
		if (not_number_pos == std::string::npos)
			return false;

		return ((!g_GameStarted || EmuConfig.HostFs) && 0 == path.compare(0, 4, "host") && path[not_number_pos] == ':');
	}

	int open_HLE()
	{
		IOManFile *file = NULL;
		const std::string path = Ra0;
		s32 flags = a1;
		u16 mode = a2;

		if (is_host(path))
		{
			if (!freefdcount())
			{
				v0 = -IOP_EMFILE;
				pc = ra;
				return 1;
			}

			int err = HostFile::open(&file, path, flags, mode);

			if (err != 0 || !file)
			{
				if (err == 0) // ???
					err = -IOP_EIO;
				if (file) // ??????
					file->close();
				v0 = err;
			}
			else
			{
				v0 = allocfd(file);
				if ((s32)v0 < 0)
					file->close();
			}

			pc = ra;
			return 1;
		}

		return 0;
	}

	int close_HLE()
	{
		s32 fd = a0;

		if (getfd<IOManFile>(fd))
		{
			freefd(fd);
			v0 = 0;
			pc = ra;
			return 1;
		}

		return 0;
	}

	int lseek_HLE()
	{
		s32 fd = a0;
		s32 offset = a1;
		s32 whence = a2;

		if (IOManFile *file = getfd<IOManFile>(fd))
		{
			v0 = file->lseek(offset, whence);
			pc = ra;
			return 1;
		}

		return 0;
	}

	int read_HLE()
	{
		s32 fd = a0;
		u32 data = a1;
		u32 count = a2;

		if (IOManFile *file = getfd<IOManFile>(fd))
		{
			try {
				std::unique_ptr<char[]> buf(new char[count]);

				v0 = file->read(buf.get(), count);

				for (s32 i = 0; i < (s32)v0; i++)
					iopMemWrite8(data + i, buf[i]);
			}
			catch (const std::bad_alloc &) {
				v0 = -IOP_ENOMEM;
			}

			pc = ra;
			return 1;
		}

		return 0;
	}

	int write_HLE()
	{
		s32 fd = a0;
		u32 data = a1;
		u32 count = a2;

		if (fd == 1) // stdout
		{
			pc = ra;
			v0 = a2;
			return 1;
		}
		else if (IOManFile *file = getfd<IOManFile>(fd))
		{
			try {
				std::unique_ptr<char[]> buf(new char[count]);

				for (u32 i = 0; i < count; i++)
					buf[i] = iopMemRead8(data + i);

				v0 = file->write(buf.get(), count);
			}
			catch (const std::bad_alloc &) {
				v0 = -IOP_ENOMEM;
			}

			pc = ra;
			return 1;
		}

		return 0;
	}
}

namespace sysmem {
	int Kprintf_HLE()
	{
		// Emulate the expected Kprintf functionality:
		iopMemWrite32(sp, a0);
		iopMemWrite32(sp + 4, a1);
		iopMemWrite32(sp + 8, a2);
		iopMemWrite32(sp + 12, a3);
		pc = ra;

		return 1;
	}
}

namespace loadcore {
	void RegisterLibraryEntries_DEBUG()
	{
	}
}

namespace intrman {
	void RegisterIntrHandler_DEBUG(void) { }
}

namespace sifcmd {
	void sceSifRegisterRpc_DEBUG(void) { }
}

u32 irxImportTableAddr(u32 entrypc)
{
	u32 i;

	i = entrypc - 0x18;
	while (entrypc - i < 0x2000) {
		if (iopMemRead32(i) == 0x41e00000)
			return i;
		i -= 4;
	}

	return 0;
}

const char* irxImportFuncname(const std::string &libname, u16 index)
{
	#include "IopModuleNames.cpp"

	switch (index) {
		case 0: return "start";
		// case 1: reinit?
		case 2: return "shutdown";
		// case 3: ???
	}

	return 0;
}

#define MODULE(n) if (#n == libname) { using namespace n; switch (index) {
#define END_MODULE }}
#define EXPORT_D(i, n) case (i): return n ## _DEBUG;
#define EXPORT_H(i, n) case (i): return n ## _HLE;

irxHLE irxImportHLE(const std::string &libname, u16 index)
{
	// debugging output
	MODULE(sysmem)
		EXPORT_H( 14, Kprintf)
	END_MODULE

	MODULE(ioman)
		EXPORT_H(  4, open)
		EXPORT_H(  5, close)
		EXPORT_H(  6, read)
		EXPORT_H(  7, write)
		EXPORT_H(  8, lseek)
	END_MODULE

	return 0;
}

irxDEBUG irxImportDebug(const std::string &libname, u16 index)
{
	MODULE(loadcore)
		EXPORT_D(  6, RegisterLibraryEntries)
	END_MODULE
	MODULE(intrman)
		EXPORT_D(  4, RegisterIntrHandler)
	END_MODULE
	MODULE(sifcmd)
		EXPORT_D( 17, sceSifRegisterRpc)
	END_MODULE

	return 0;
}

#undef MODULE
#undef END_MODULE
#undef EXPORT_D
#undef EXPORT_H

int irxImportExec(u32 import_table, u16 index)
{
	if (!import_table)
		return 0;

	std::string libname = iopMemReadString(import_table + 12, 8);
	irxImportFuncname(libname, index);
	irxHLE hle = irxImportHLE(libname, index);
	irxDEBUG debug = irxImportDebug(libname, index);

	if (debug)
		debug();
	
	if (hle)
		return hle();
	else
		return 0;
}

}	// end namespace R3000A
