#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "texture_atlas.h"

#define ATLAS_MIN_RESERVED_HOLES 32
#define ATLAS_MIN_RESERVED_VTEXES 32

// Trivial Rectangle, containing either free space or a virtual texture.
typedef struct Rect {
    uint16_t left, up;
    uint16_t right, down;
} Rect;

/**
 * Contains virtual texture metadata. Actual texel is an implementation detail
 * of the library user.
 * @property rect: Rectangle containing the virtual texture and padding.
 * @property id: Unique identifier for the virtual texture.
 * @property invalidated: Tags virtual texture for deletion upon next upload.
 **/
typedef struct VirtualTexture {
        Rect rect;
        uint32_t id;
        int invalidated;
} VirtualTexture;

typedef struct Atlas {
    /**
     * Holes describe areas in the atlas that are empty. A hole can overlap
     * other holes, but not fully contain another.
     **/
    Rect *holes;
    uint16_t hole_count; // Currently created holes.
    uint16_t hole_reserved; 
    int holes_invalidated; // Whether the hole structure was invalidated.


    /**
     * Virtual Textures meta-data. Describes how and where texel data is pinned 
     * to the altas page.
     **/
    VirtualTexture *vtexes;
    uint16_t vtex_count;
    uint16_t vtex_last_id;
    uint16_t vtex_reserved;

    uint16_t padding; // Padding to be added to the borders of every virtual texture.
    uint16_t dimensions; // Atlas page dimensions.
} Atlas;

static inline int rect_width(Rect *rect)
{
    int val = rect->right - rect->left;
    return val < 0 ? 0 : val;
}

static inline int rect_height(Rect *rect)
{
    int val = rect->down - rect->up;
    return val < 0 ? 0 : val;
}

static inline int rect_area(Rect *rect)
{
    return rect_width(rect) * rect_height(rect);
}

/**
 * Private, look-up the index for a given virtual texture id.
 * @arg atlas: Pointer to atlas structure.
 * @arg id: Unique virtual texture identifier.
 * @returns: Index of the texture atlas slot, otherwise -1.
 **/
static int atlas_lookup_vtex_id(Atlas *atlas, uint32_t id)
{
    int i;
    for (i = 0; i < atlas->vtex_count; i++) {
        if (atlas->vtexes[i].id == id)
            break;
    }

    if (i == atlas->vtex_count)
        return -1;
    return i;
}

/**
 * Private, look-up the smallest possible rectangle where the texture fits.
 * @arg atlas: Pointer to atlas structure.
 * @arg w: Texture width.
 * @arg h: Texture height.
 * @returns: Pointer to the hole Rect if successful, otherwise NULL.
 **/
static Rect *atlas_lookup_bestfit(Atlas *atlas, int w, int h)
{
    Rect *last_best = NULL;
    uint32_t last_best_area = UINT32_MAX;
    for (int i = 0; i < atlas->hole_count; i++) {
        Rect *hole = &atlas->holes[i];
        if (rect_width(hole) < w || rect_height(hole) < h)
            continue;

        uint32_t area = rect_area(hole);
        if (area < last_best_area) {
            last_best = hole;
            last_best_area = area;
        }
    }

    return last_best;
}

/**
 * Private, copy Rect b into a.
 * @arg a: Rect to be overwriten.
 * @arg b: Rect to be copied from, left intact.
 **/
static void rect_copy(Rect *a, const Rect *b)
{
    if (a == b)
        return;

    a->left = b->left;
    a->up = b->up;
    a->right = b->right;
    a->down = b->down;
}

/**
 * Private, reserves more atlas holes array space.
 * @arg atlas: Pointer to atlas structure.
 * @arg reserved: Number of holes to be reserved.
 * @return: 1 on success, 0 otherwise.
 **/
static int atlas_reserve_holes(Atlas *atlas, int reserved)
{
    Rect *holes = (Rect*)realloc(atlas->holes, sizeof(holes[0]) * reserved);
    if (!holes)
        return 0;
    
    atlas->holes = holes;
    atlas->hole_reserved = reserved;
    return 1;
}

/**
 * Private, reserves more atlas virtual texture array space.
 * @arg atlas: Pointer to atlas structure.
 * @arg reserved: Number of virtual textures to be reserved.
 * @return: 1 on success, 0 otherwise.
 **/
static int atlas_reserve_vtexes(Atlas *atlas, int reserved)
{
    VirtualTexture *vtexes = (VirtualTexture*)realloc(atlas->vtexes, sizeof(vtexes[0]) * reserved);
    if (!vtexes)
        return 0;
    
    atlas->vtexes = vtexes;
    atlas->vtex_reserved = reserved;
    return 1;
}

/**
 * Private, resets hole count to 1 and resets first hole.
 * @arg atlas: Pointer to atlas structure.
 **/
static void atlas_reset_holes(Atlas *atlas)
{
    Rect first = {0, 0, atlas->dimensions, atlas->dimensions};
    rect_copy(&atlas->holes[0], &first);
    atlas->hole_count = 1;
}

/**
 * Creates and populate atlas structure.
 * @arg atlas_ptr: Double pointer to atlas structure, undefined on failure.
 * @arg dimensions: Defines atlas width and height dimenions.
 * @arg padding: Defines padding added to all sides of a virtual texture.
 * @return: 1 on success, 0 otherwise.
 **/
int atlas_create(Atlas **atlas_dptr, uint16_t dimensions, uint16_t padding)
{
    Atlas *atlas = (Atlas*)calloc(1, sizeof(*atlas));
    if (!atlas)
        goto err_allocate;

    // Attempt to reserve space for the necessary meta-data structures.
    if (!atlas_reserve_holes(atlas, ATLAS_MIN_RESERVED_HOLES) || 
        !atlas_reserve_vtexes(atlas, ATLAS_MIN_RESERVED_VTEXES))
        goto err_reserve;

    atlas->vtex_last_id = 1;
    atlas->dimensions = dimensions;
    atlas->padding = padding;

    // Initializes first hole
    atlas_reset_holes(atlas);

    *atlas_dptr = atlas;
    return 1;
err_reserve:
    atlas_destroy(atlas);
err_allocate:
    return 0;
}

/**
 * Free atlas and all data allocated by the atlas.
 * @arg atlas: Pointer to private Atlas structure.
 */
void atlas_destroy(Atlas *atlas)
{
    if (atlas->holes)
        free(atlas->holes);
    if (atlas->vtexes)
        free(atlas->vtexes);
        
    free(atlas);
}

/**
 * Acquires a virtual texture slot.
 * @arg atlas: Pointer to private Atlas structure.
 * @arg id_ptr: Pointer to retrieve virtual texture slot id.
 * @return: 1 on success, 0 otherwise.
 **/
int atlas_gen_texture(Atlas *atlas, uint32_t *id_ptr)
{
    // If we don't have enough virtual texture slots reserved, attempt to double
    // the number of reserved slots.
    if (atlas->vtex_count >= atlas->vtex_reserved) {
        if (!atlas_reserve_vtexes(atlas, atlas->vtex_reserved * 2)) {
            return 0;
        }
    }

    // Acquire an unique ID and a reusable virtual texture slot.
    VirtualTexture *vt = &atlas->vtexes[atlas->vtex_count++];
    vt->id = atlas->vtex_last_id++;
    vt->invalidated = 0;
    vt->rect.left = vt->rect.up = vt->rect.right = vt->rect.down = 0;

    *id_ptr = vt->id;
    return 1;
}

/**
 * Private, checks if two rects have any overlap.
 * @param a: Pointer to first Rect.
 * @param b: Pointer to second Rect.
 * @returns: 1 if overlaps, 0 otherwise.
 **/
static int rect_overlaps(Rect *a, Rect *b)
{
    return ((a->right > b->left) && (a->left < b->right)) &&
           ((a->up    < b->down) && (a->down > b->up   ));
}

/**
 * Private, checks if a is completely contained within b.
 * @param a: Pointer to first Rect.
 * @param b: Pointer to second Rect.
 * @returns: 1 if overlaps, 0 otherwise.
 **/
static int rect_contained(Rect *a, Rect *b)
{
    return ((a->left >= b->left) && (a->right <= b->right)) && 
           ((a->up   >= b->up  ) && (a->down  <= b->down ));
}

/**
 * Private, splits all atlas holes overlapped by the rectangle.
 * @param atlas: Pointer to private Atlas structure.
 * @param x: Left rectangle coordinate.
 * @param y: Top rectangle coordinate.
 * @param w: Rectangle width.
 * @param h: Rectangle height.
 * @return: 1 on success, 0 otherwise. Failure means structure is left in an invalid state.
 **/
static int atlas_split_holes(Atlas *atlas, Rect *cut)
{
    for (int i = 0; i < atlas->hole_count; i++) {
        Rect *hole = &atlas->holes[i];
        if (!rect_overlaps(cut, hole))
            continue;

        // New Rect splits to be considered for emplacing
        Rect new_holes[4] = {
            /* Up    */ {hole->left, hole->up,  hole->right, cut->up   },
            /* Down  */ {hole->left, cut->down, hole->right, hole->down},
            /* Left  */ {hole->left, hole->up,  cut->left,   hole->down},
            /* Right */ {cut->right, hole->up,  hole->right, hole->down},
        };

        // Deallocate current hole
        rect_copy(&atlas->holes[i], &atlas->holes[--atlas->hole_count]);

        for (int j = 0; j < 4; j++) {
            // Skip zero area holes.
            if (rect_area(&new_holes[j]) == 0)
                continue;

            // If we don't have enough hole slots, reserve more.
            if (atlas->hole_count == atlas->hole_reserved) {
                if (!atlas_reserve_holes(atlas, atlas->hole_reserved * 2))
                    return 0;
            }

            // Emplace the new hole.
            rect_copy(&atlas->holes[atlas->hole_count++], &new_holes[j]);
        }

        // For every (j, k) pair of Rects: 
        //      if j fully contained in k, remove j
        // else if k fully contained in j, remove k
        for (int j = 0; j < atlas->hole_count; j++) {
            for (int k = j + 1; k < atlas->hole_count; k++) {
                if (rect_contained(&atlas->holes[k], &atlas->holes[j])) {
                    // Move last Hole index into k and drop k
                    rect_copy(&atlas->holes[k], &atlas->holes[--atlas->hole_count]);

                    // Since we moved the last Hole into k, we need to recheck the
                    // k index instead of advancing into k+1
                    k--;
                } else if (rect_contained(&atlas->holes[j], &atlas->holes[k])) {
                    // Move last Hole index into j and drop j
                    rect_copy(&atlas->holes[j], &atlas->holes[--atlas->hole_count]);

                    // Since we moved into j, we will need to reset k now
                    k = j + 1;
                }
            }
        }

        // Retry current hole index.
        i--;
    }

    return 1;
}

/**
 * Marks a virtual texture for future reclaiming. This happens the next time a
 * texture gets uploaded.
 * @arg atlas: Pointer to private Atlas structure.
 * @arg id: Unique virtual texture identifier.
 * @return: 1 on success, 0 otherwise.
 **/
int atlas_destroy_vtex(Atlas *atlas, uint32_t id)
{
    int index;
    if ((index = atlas_lookup_vtex_id(atlas, id)) == -1)
        return 0;

    atlas->vtexes[index].invalidated = 1;
    atlas->holes_invalidated = 1;

    return 1;
}

/**
 * Allocates space for the virtual texture.
 * @arg atlas: Pointer to private Atlas structure.
 * @arg id: Unique virtual texture identifier.
 * @arg w: Virtual texture width.
 * @arg h: Virtual texture height.
 * @return: 1 on success, 0 otherwise.
 **/
int atlas_allocate_vtex_space(Atlas *atlas, uint32_t id, uint16_t w, uint16_t h)
{
    // If a texture has been deleted, we'll regenerate the holes before
    // trying to allocate space for a new one.
    // TODO:: Benchmark impact of this
    if (atlas->holes_invalidated) {
        atlas->holes_invalidated = 0;
        atlas_reset_holes(atlas);

        // Delete all invalidated textures
        for (int i = 0; i < atlas->vtex_count; i++) {
            VirtualTexture *vt = &atlas->vtexes[i];
            
            if (!vt->invalidated) {
                // Ignore textures that haven't had space allocated for them
                if (rect_area(&vt->rect) == 0)
                    continue;

                // If virtual texture isn't invalidated, let's reallocate space for it
                atlas_split_holes(atlas, &vt->rect);
            } else {
                // Otherwise, swap current virtual texture for the last entry
                VirtualTexture *last = &atlas->vtexes[atlas->vtex_count - 1];
                rect_copy(&vt->rect, &last->rect);
                vt->id = last->id;
                vt->invalidated = last->invalidated;

                // Remove last virtual texture and retry current index
                atlas->vtex_count--;
                i--;
            } 
        }
    }

    // Add padding.
    w += atlas->padding * 2;
    h += atlas->padding * 2;

    // Look-up virtual texture id
    VirtualTexture *vt = NULL;
    for (int i = 0; i < atlas->vtex_count; i++) {
        if (atlas->vtexes[i].id == id) {
            vt = &atlas->vtexes[i];
            break;
        }
    }

    // If id not found, bail out
    if (!vt)
        return 0;

    // Do a best-fit lookup
    Rect *best_fit = atlas_lookup_bestfit(atlas, w, h);
    if (!best_fit)
        return 0;

    // Split holes as necessary
    Rect vtex = {
        /* left, up    */ best_fit->left,     best_fit->up, 
        /* right, down */ best_fit->left + w, best_fit->up + h
    };

    rect_copy(&vt->rect, &vtex);
    if (!atlas_split_holes(atlas, &vtex))
        return 0;

    return 1;
}

/**
 * Retrieves normalized texture coordinates (u, v) and (s, t) for a given unique
 * virtual texture id.
 * @arg atlas: Pointer to private Atlas structure.
 * @arg id: Unique virtual texture identifier.
 * @arg uvst: Pointer to retrieve (u, v) and (s, t) normalized coordinates.
 * @return: 1 if virtual texture id is valid, 0 otherwise.
 **/
int atlas_get_vtex_uvst_coords(Atlas *atlas, uint32_t id, int padding, float *uvst)
{
    int index;
    if ((index = atlas_lookup_vtex_id(atlas, id)) == -1)
        return 0;

    Rect *vt = &atlas->vtexes[index].rect;
    uvst[0] = (float)(vt->left ) / atlas->dimensions;
    uvst[1] = (float)(vt->up   ) / atlas->dimensions;
    uvst[2] = (float)(vt->right) / atlas->dimensions;
    uvst[3] = (float)(vt->down ) / atlas->dimensions;

    if (!padding) {
        float atlas_norm_padding = (float)atlas->padding / atlas->dimensions;
        uvst[0] += atlas->padding / atlas_norm_padding;
        uvst[1] += atlas->padding / atlas_norm_padding;
        uvst[2] -= atlas->padding / atlas_norm_padding;
        uvst[3] -= atlas->padding / atlas_norm_padding;
    }

    return 1;
}

/**
 * Retrieves texture coordinates (x, y) and (w, h) for a given unique
 * virtual texture id.
 * @arg atlas: Pointer to private Atlas structure.
 * @arg id: Unique virtual texture identifier.
 * @arg uvst: Pointer to retrieve (x, y) and (w, h) coordinates.
 * @return: 1 if virtual texture id is valid, 0 otherwise.
 **/
int atlas_get_vtex_xywh_coords(Atlas *atlas, uint32_t id, int padding, uint16_t *xywh)
{
    int index;
    if ((index = atlas_lookup_vtex_id(atlas, id)) == -1)
        return 0;

    Rect *vt = &atlas->vtexes[index].rect;
    xywh[0] =  vt->left;
    xywh[1] =  vt->up;
    xywh[2] = (vt->right - vt->left);
    xywh[3] = (vt->down  - vt->up  );

    if (!padding) {
        xywh[0] +=  atlas->padding;
        xywh[1] +=  atlas->padding;
        xywh[2] -= (atlas->padding * 2);
        xywh[3] -= (atlas->padding * 2);
    }

    return 1;
}

/**
 * Retrieves atlas dimensions.
 * @arg atlas: Pointer to private Atlas structure.
 * @returns: Atlas dimensions.
 **/
uint16_t atlas_get_dimensions(Atlas *atlas)
{
    return atlas->dimensions;
}

/**
 * Retrieves atlas padding.
 * @arg atlas: Pointer to private Atlas structure.
 * @returns: Atlas dimensions.
 **/
uint16_t atlas_get_padding(Atlas *atlas)
{
    return atlas->padding;
}