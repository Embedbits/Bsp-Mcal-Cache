/**
 * \author Mr.Nobody
 * \file Cache_Port.h
 * \ingroup Cache
 * \brief CPU cache (instruction and data cache) MCAL module public functionality
 *
 * This file contains all available public functionality, any other files shall
 * not used outside of the module. The interface is the same for all families.
 *
 */

#ifndef CACHE_CACHE_PORT_H
#define CACHE_CACHE_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "Cache_Types.h"                    /* Module types definition        */
/* ============================== TYPEDEFS ================================== */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ========================== EXPORTED MACROS =============================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */

cache_ModuleVersion_t   Cache_Get_ModuleVersion     ( void );

cache_RequestState_t    Cache_Init                  ( const cache_Config_t * const cacheConfig );
cache_RequestState_t    Cache_Deinit                ( void );
void                    Cache_Task                  ( void );

cache_RequestState_t    Cache_Get_DefaultConfig     ( cache_Config_t * const cacheConfig );

/*---------------------------- Requested state -------------------------------*/

cache_RequestState_t    Cache_Set_Active            ( cache_CacheId_t cacheId );
cache_RequestState_t    Cache_Set_Inactive          ( cache_CacheId_t cacheId );
cache_RequestState_t    Cache_Get_State             ( cache_CacheId_t cacheId, cache_State_t * const cacheState );

/*---------------------------- Temporary bypass ------------------------------*/

cache_RequestState_t    Cache_Set_Suspend           ( cache_CacheId_t cacheId );
cache_RequestState_t    Cache_Set_Resume            ( cache_CacheId_t cacheId );

/*------------------------------ Maintenance ---------------------------------*/

cache_RequestState_t    Cache_Set_Invalidate        ( cache_CacheId_t cacheId );

cache_RequestState_t    Cache_Set_CleanRange              ( cache_CacheId_t cacheId, uint32_t address, uint32_t size );
cache_RequestState_t    Cache_Set_InvalidateRange         ( cache_CacheId_t cacheId, uint32_t address, uint32_t size );
cache_RequestState_t    Cache_Set_CleanInvalidateRange    ( cache_CacheId_t cacheId, uint32_t address, uint32_t size );

/*------------------------------- Monitors -----------------------------------*/

cache_RequestState_t    Cache_Get_Monitor           ( cache_CacheId_t cacheId, cache_Monitor_t * const cacheMonitor );
cache_RequestState_t    Cache_Set_MonitorReset      ( cache_CacheId_t cacheId );

#ifdef __cplusplus
}
#endif

#endif /* CACHE_CACHE_PORT_H */
