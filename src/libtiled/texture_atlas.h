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
