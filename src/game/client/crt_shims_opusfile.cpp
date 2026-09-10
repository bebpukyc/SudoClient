/* Compatibility shims for opusfile.lib, which was built against an older MSVC
 * CRT that exported _wfreopen / _filelengthi64 (via the OS msvcrt.dll import
 * library). The VS2022 UCRT no longer provides these symbols, so we supply
 * implementations here and publish the __imp_ thunks opusfile references. */
#include <stdio.h>
#include <share.h>
#include <io.h>
#include <wchar.h>

extern "C" FILE *__cdecl _wfreopen(const wchar_t *pPath, const wchar_t *pMode, FILE *pStream)
{
	if(pStream)
		fclose(pStream);
	return _wfsopen(pPath, pMode, _SH_DENYNO);
}

extern "C" __int64 __cdecl _filelengthi64(int Fd)
{
	__int64 Cur = _lseeki64(Fd, 0, SEEK_CUR);
	__int64 End = _lseeki64(Fd, 0, SEEK_END);
	_lseeki64(Fd, Cur, SEEK_SET);
	return End;
}

typedef FILE *(__cdecl *TFreopen)(const wchar_t *, const wchar_t *, FILE *);
typedef __int64(__cdecl *TFilelengthi64)(int);

extern "C" TFreopen __imp__wfreopen = _wfreopen;
extern "C" TFilelengthi64 __imp__filelengthi64 = _filelengthi64;
