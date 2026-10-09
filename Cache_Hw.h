/**
 * \author Mr.Nobody
 * \file Cache_Hw.h
 * \ingroup Cache
 * \brief CPU cache (instruction and data cache) MCAL module hardware level functionality header file.
 *
 * Private functions that work with the cache peripherals of the device through the LL (ICACHE, DCACHE1).
 * Nothing from this file is available outside of the module.
 *
 */

#ifndef CACHE_CACHE_HW_H
#define CACHE_CACHE_HW_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Cache_Types.h"                    /* Module types definitions       */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Count of the iterations of the busy-wait loops (disable and invalidation of a cache) */
#define CACHE_HW_TIMEOUT_CNT                ( 1000000u )

/* ============================= TYPEDEFS =================================== */

/** Type representing iteration count of busy-wait loops */
typedef uint32_t cache_TimeoutCnt_t;

/* ========================= EXPORTED FUNCTIONS ============================= */

cache_FlagState_t       Cache_Hw_Get_Present        ( cache_CacheId_t cacheId );

cache_FlagState_t       Cache_Hw_Get_Enabled        ( cache_CacheId_t cacheId );
cache_RequestState_t    Cache_Hw_Set_Enabled        ( cache_CacheId_t cacheId, cache_FlagState_t enabled );

cache_Ways_t            Cache_Hw_Get_Ways           ( cache_CacheId_t cacheId );
void                    Cache_Hw_Set_Ways           ( cache_CacheId_t cacheId, cache_Ways_t ways );

cache_RequestState_t    Cache_Hw_Invalidate         ( cache_CacheId_t cacheId );

cache_FlagState_t       Cache_Hw_Get_MonitorActive  ( cache_CacheId_t cacheId );
void                    Cache_Hw_Set_MonitorActive  ( cache_CacheId_t cacheId, cache_FlagState_t active );
void                    Cache_Hw_Get_Monitor        ( cache_CacheId_t cacheId, cache_Monitor_t * const cacheMonitor );
void                    Cache_Hw_Set_MonitorReset   ( cache_CacheId_t cacheId );

#ifdef __cplusplus
}
#endif

#endif /* CACHE_CACHE_HW_H */
