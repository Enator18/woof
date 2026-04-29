#ifndef VK_INIT_H
#define VK_INIT_H

#include "doomtype.h"

#ifndef EXTERN_C
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif
#endif

EXTERN_C void VK_Init(byte* paletteData);

EXTERN_C void VK_InitSwapchain(uint32_t width, uint32_t height, boolean vsync);
EXTERN_C void VK_DestroySwapchain();

EXTERN_C void VK_CreateFramebuffers(uint32_t width, uint32_t height);
EXTERN_C void VK_DestroyFramebuffers();

#endif //VK_INIT_H
