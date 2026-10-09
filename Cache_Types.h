/**
 * \defgroup Cache Cache
 * \brief Cache module
 */

/**
 * \author Mr.Nobody
 * \file Cache_Types.h
 * \ingroup Cache
 * \brief CPU cache (instruction and data cache) module global types definition
 *
 * This file contains the types definitions used across the module and are
 * available for other modules through Port file. The types are the same for all
 * families (see the README of the repository).
 *
 */

#ifndef CACHE_CACHE_TYPES_H
#define CACHE_CACHE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define CACHE_NULL_PTR                      ( ( void* ) 0u )

/** Maximal value of the suspend counter of a cache */
#define CACHE_SUSPEND_CNT_MAX               ( 255u )

/* ============================== TYPEDEFS ================================== */

/** \brief Type signaling major version of SW module */
typedef uint8_t cache_MajorVersion_t;


/** \brief Type signaling minor version of SW module */
typedef uint8_t cache_MinorVersion_t;


/** \brief Type signaling patch version of SW module */
typedef uint8_t cache_PatchVersion_t;


/** \brief Type signaling actual version of SW module */
typedef struct
{
    cache_MajorVersion_t Major; /**< Major version */
    cache_MinorVersion_t Minor; /**< Minor version */
    cache_PatchVersion_t Patch; /**< Patch version */
}   cache_ModuleVersion_t;


/** Function status enumeration */
typedef enum
{
    CACHE_FUNCTION_INACTIVE = 0u, /**< Function status is inactive */
    CACHE_FUNCTION_ACTIVE         /**< Function status is active   */
}   cache_FunctionState_t;


/** Flag states enumeration */
typedef enum
{
    CACHE_FLAG_INACTIVE = 0u, /**< Inactive flag state */
    CACHE_FLAG_ACTIVE         /**< Active flag state   */
}   cache_FlagState_t;


/** Enumeration used to signal request processing state */
typedef enum
{
    CACHE_REQUEST_ERROR = 0u, /**< Processing request failed  */
    CACHE_REQUEST_OK          /**< Processing request succeed */
}   cache_RequestState_t;


/** Identification of a cache of the device */
typedef enum
{
    CACHE_INSTRUCTION = 0u, /**< Cache of the code and of the constants read from the flash */
    CACHE_DATA,             /**< Cache of the data (where the device has one)               */
    CACHE_ID_CNT            /**< Count of the caches                                        */
}   cache_CacheId_t;


/** Count of the users that need a cache suspended (disabled) */
typedef uint8_t cache_SuspendCnt_t;


/** Associativity of a cache */
typedef enum
{
    CACHE_WAYS_1 = 0u, /**< 1-way cache (direct mapped)       */
    CACHE_WAYS_2       /**< 2-ways set associative cache      */
}   cache_Ways_t;


/** State of a cache */
typedef struct
{
    cache_FlagState_t  Requested;  /**< The application wants the cache enabled                          */
    cache_FlagState_t  Enabled;    /**< The cache is enabled in the hardware (requested and not suspended) */
    cache_SuspendCnt_t SuspendCnt; /**< Count of the users that need the cache suspended                 */
}   cache_State_t;


/** Hit and miss monitors of a cache */
typedef struct
{
    uint32_t Hit;  /**< Count of the hits   */
    uint32_t Miss; /**< Count of the misses */
}   cache_Monitor_t;


/** Configuration of one cache */
typedef struct
{
    cache_FlagState_t Requested;     /**< The cache shall be enabled                                       */
    cache_Ways_t      Ways;          /**< Associativity (only caches with a selectable one, else ignored) */
    cache_FlagState_t MonitorActive; /**< The hit and miss monitors count                                  */
}   cache_CacheConfig_t;


/** Configuration of the module */
typedef struct
{
    cache_CacheConfig_t Cache[ CACHE_ID_CNT ]; /**< Configuration of every cache */
}   cache_Config_t;

#ifdef __cplusplus
}
#endif

#endif /* CACHE_CACHE_TYPES_H */
