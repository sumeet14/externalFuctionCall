#ifndef __external_h__
#define __external_h__

//typedef unsigned char bool;

#ifdef _cplusplus
extern "C" {
#endif
	
__declspec (dllexport) __stdcall int addNumbers(int a, int b);

#ifdef _cplusplus
}
#endif
#endif