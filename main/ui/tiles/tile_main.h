#pragma once

#include "tile_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the main landing page tile
 * @param parent The parent tileview object
 * @return Pointer to the created tile object
 */
lv_obj_t* tile_main_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif