/*
Modified BSD License
====================

_Copyright © 2020, João Henrique_
_All rights reserved._

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL JOÃO HENRIQUE BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __TEXTURE_ATLAS_H__
#define __TEXTURE_ATLAS_H__

#include "tiled_global.h"

#ifdef __cplusplus
#include <cstdint>
extern "C"
{
#endif
    typedef struct Atlas Atlas;

    extern TILEDSHARED_EXPORT int atlas_create(Atlas **atlas_dptr, uint16_t dimensions, uint16_t padding);
    extern TILEDSHARED_EXPORT void atlas_destroy(Atlas *atlas);
    extern TILEDSHARED_EXPORT int atlas_gen_texture(Atlas *atlas, uint32_t *id_ptr);
    extern TILEDSHARED_EXPORT int atlas_destroy_vtex(Atlas *atlas, uint32_t id);
    extern TILEDSHARED_EXPORT int atlas_allocate_vtex_space(Atlas *atlas, uint32_t id, uint16_t w, uint16_t h);
    extern TILEDSHARED_EXPORT int atlas_get_vtex_uvst_coords(Atlas *atlas, uint32_t id, int padding, float *uvst);
    extern TILEDSHARED_EXPORT int atlas_get_vtex_xywh_coords(Atlas *atlas, uint32_t id, int padding, uint16_t *xywh);
    extern TILEDSHARED_EXPORT uint16_t atlas_get_dimensions(Atlas *atlas);
    extern TILEDSHARED_EXPORT uint16_t atlas_get_padding(Atlas *atlas);
#ifdef __cplusplus
}
#endif

#endif /* __TEXTURE_ATLAS__ */
