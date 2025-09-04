#pragma once

#include "tile_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the updates tile with OTA button and progress
 * @param parent The parent tileview object
 * @return Pointer to the created tile object
 */
lv_obj_t* tile_updates_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif