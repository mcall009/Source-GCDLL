/**
 * GCDLL.h - Header file for the GCDLL encryption library
 * Compatible with CryptLib.pas interface
 */

#ifndef GCDLL_H
#define GCDLL_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

	// Function prototypes matching the Delphi interface in CryptLib.pas
	__declspec(dllexport) char* __stdcall _Encrypt(const char* data, const char* iv, unsigned char rnd);
	__declspec(dllexport) char* __stdcall _Decrypt(const char* data, const char* iv);
	__declspec(dllexport) char* __stdcall _GenerateIV(const char* ivHash, DWORD ivType);
	__declspec(dllexport) char* __stdcall _ClearPacket(const char* data, const char* iv2);

#ifdef __cplusplus
}
#endif

#endif // GCDLL_H