// Android (API 24) has no iconv, so libvgm's charset conversion is stubbed out there.
// CPConv_Init failing makes the players skip tag conversion; playback is unaffected.
#ifdef __ANDROID__

#include "utils/StrUtils.h"

UINT8 CPConv_Init(CPCONV** retCPC, const char* cpFrom, const char* cpTo) {
	(void)cpFrom;
	(void)cpTo;
	*retCPC = NULL;
	return 0xFF;
}

void CPConv_Deinit(CPCONV* cpc) {
	(void)cpc;
}

UINT8 CPConv_StrConvert(CPCONV* cpc, size_t* outSize, char** outStr, size_t inSize, const char* inStr) {
	(void)cpc;
	(void)inSize;
	(void)inStr;
	*outSize = 0;
	(void)outStr;
	return 0xFF;
}

#endif
