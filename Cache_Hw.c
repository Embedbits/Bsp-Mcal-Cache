/**
 * \author Mr.Nobody
 * \file Cache_Hw.c
 * \ingroup Cache
 * \brief CPU cache (instruction and data cache) MCAL module hardware level functionality
 *
 * STM32H5: the instruction cache ICACHE (all lines) and the data cache DCACHE1 (the lines that have it).
 * The sequences follow the ST HAL: the disabling of the ICACHE starts an invalidation of its content (the
 * bit EN reads 1 until it is finished), the invalidation is started only when the cache is not busy.
 *
 * \note Register write verification (read-back): the enable bit of both caches is verified, the associativity
 *       and the monitor bits of the ICACHE and DCACHE1 are plain configuration bits. Exceptions: the invalidation
 *       request (CACHEINV is cleared by the controller), the monitor reset bits (self clearing) and the flags
 *       (write 1 to clear).
 *
 */
/* ============================== INCLUDES ================================== */
#include "Cache_Hw.h"                       /* Self include                   */
#include "Cache_Types.h"                    /* Module types definitions       */
#include "Stm32_icache.h"                   /* Instruction cache functionality*/
#include "Stm32_dcache.h"                   /* Data cache functionality       */
/* ======================= FORWARD DECLARATIONS ============================= */

static cache_RequestState_t Cache_Hw_Set_IcacheEnabled      ( cache_FlagState_t enabled );
static cache_RequestState_t Cache_Hw_Invalidate_Icache      ( void );
#if defined(DCACHE1)
static cache_RequestState_t Cache_Hw_Set_DcacheEnabled      ( cache_FlagState_t enabled );
static cache_RequestState_t Cache_Hw_Invalidate_Dcache      ( void );
static cache_FlagState_t    Cache_Hw_Get_DcacheBusy         ( void );
#endif /* DCACHE1 */

/* ======================== EXPORTED FUNCTIONS ============================== */

/**
 * \brief Reads whether the device has the cache.
 *
 * \param cacheId: Cache
 *
 * \return \ref CACHE_FLAG_ACTIVE if the cache exists
 */
cache_FlagState_t Cache_Hw_Get_Present( cache_CacheId_t cacheId )
{
    cache_FlagState_t presentState = CACHE_FLAG_INACTIVE;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        presentState = CACHE_FLAG_ACTIVE;
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        presentState = CACHE_FLAG_ACTIVE;
    }
#endif /* DCACHE1 */
    else
    {
        presentState = CACHE_FLAG_INACTIVE;
    }

    return (presentState);
}


/**
 * \brief Reads whether the cache is enabled in the hardware.
 *
 * \param cacheId: Cache (it exists)
 *
 * \return \ref CACHE_FLAG_ACTIVE if the cache is enabled
 */
cache_FlagState_t Cache_Hw_Get_Enabled( cache_CacheId_t cacheId )
{
    cache_FlagState_t enabledState = CACHE_FLAG_INACTIVE;
    uint32_t          enabled      = 0u;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        enabled = LL_ICACHE_IsEnabled();
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        enabled = LL_DCACHE_IsEnabled( DCACHE1 );
    }
#endif /* DCACHE1 */
    else
    {
        enabled = 0u;
    }

    if ( 0u != enabled )
    {
        enabledState = CACHE_FLAG_ACTIVE;
    }

    return (enabledState);
}


/**
 * \brief Enables or disables a cache and waits until the hardware has done it.
 *
 * Disabling the instruction cache invalidates its content.
 *
 * \param cacheId: Cache (it exists)
 * \param enabled: \ref CACHE_FLAG_ACTIVE to enable, \ref CACHE_FLAG_INACTIVE to disable
 *
 * \return \ref CACHE_REQUEST_OK if the cache is in the required state, otherwise \ref CACHE_REQUEST_ERROR
 */
cache_RequestState_t Cache_Hw_Set_Enabled( cache_CacheId_t cacheId, cache_FlagState_t enabled )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        retState = Cache_Hw_Set_IcacheEnabled( enabled );
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        retState = Cache_Hw_Set_DcacheEnabled( enabled );
    }
#endif /* DCACHE1 */
    else
    {
        retState = CACHE_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Reads the associativity of a cache.
 *
 * \param cacheId: Cache (it exists)
 *
 * \return Associativity (\ref CACHE_WAYS_2 for a cache without a selectable one)
 */
cache_Ways_t Cache_Hw_Get_Ways( cache_CacheId_t cacheId )
{
    cache_Ways_t ways = CACHE_WAYS_2;
    uint32_t     mode;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        mode = LL_ICACHE_GetMode();

        if ( LL_ICACHE_1WAY == mode )
        {
            ways = CACHE_WAYS_1;
        }
    }

    return (ways);
}


/**
 * \brief Sets the associativity of a cache (the instruction cache has to be disabled, the data cache has none).
 *
 * \param cacheId: Cache (it exists)
 * \param ways: Associativity
 */
void Cache_Hw_Set_Ways( cache_CacheId_t cacheId, cache_Ways_t ways )
{
    if ( CACHE_INSTRUCTION == cacheId )
    {
        if ( CACHE_WAYS_1 == ways )
        {
            LL_ICACHE_SetMode( LL_ICACHE_1WAY );
        }
        else
        {
            LL_ICACHE_SetMode( LL_ICACHE_2WAYS );
        }
    }
}


/**
 * \brief Invalidates a cache and waits for the end of the invalidation.
 *
 * \param cacheId: Cache (it exists)
 *
 * \return \ref CACHE_REQUEST_OK if the invalidation is finished, otherwise \ref CACHE_REQUEST_ERROR (busy or timeout)
 */
cache_RequestState_t Cache_Hw_Invalidate( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        retState = Cache_Hw_Invalidate_Icache();
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        retState = Cache_Hw_Invalidate_Dcache();
    }
#endif /* DCACHE1 */
    else
    {
        retState = CACHE_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Reads whether the hit and miss monitors of a cache count.
 *
 * \param cacheId: Cache (it exists)
 *
 * \return \ref CACHE_FLAG_ACTIVE if the monitors count
 */
cache_FlagState_t Cache_Hw_Get_MonitorActive( cache_CacheId_t cacheId )
{
    cache_FlagState_t activeState = CACHE_FLAG_INACTIVE;
    uint32_t          active      = 0u;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        active = LL_ICACHE_IsEnabledMonitors( LL_ICACHE_MONITOR_ALL );
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        active = LL_DCACHE_IsEnabledMonitors( DCACHE1, LL_DCACHE_MONITOR_ALL );
    }
#endif /* DCACHE1 */
    else
    {
        active = 0u;
    }

    if ( 0u != active )
    {
        activeState = CACHE_FLAG_ACTIVE;
    }

    return (activeState);
}


/**
 * \brief Activates or deactivates the hit and miss monitors of a cache.
 *
 * \param cacheId: Cache (it exists)
 * \param active: \ref CACHE_FLAG_ACTIVE to count
 */
void Cache_Hw_Set_MonitorActive( cache_CacheId_t cacheId, cache_FlagState_t active )
{
    if ( CACHE_INSTRUCTION == cacheId )
    {
        if ( CACHE_FLAG_ACTIVE == active )
        {
            LL_ICACHE_EnableMonitors( LL_ICACHE_MONITOR_ALL );
        }
        else
        {
            LL_ICACHE_DisableMonitors( LL_ICACHE_MONITOR_ALL );
        }
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        if ( CACHE_FLAG_ACTIVE == active )
        {
            LL_DCACHE_EnableMonitors( DCACHE1, LL_DCACHE_MONITOR_ALL );
        }
        else
        {
            LL_DCACHE_DisableMonitors( DCACHE1, LL_DCACHE_MONITOR_ALL );
        }
    }
#endif /* DCACHE1 */
    else
    {
        /* no monitors */
    }
}


/**
 * \brief Reads the hit and miss monitors of a cache (the data cache: reads and writes together).
 *
 * \param cacheId: Cache (it exists)
 * \param cacheMonitor [out]: Counters
 */
void Cache_Hw_Get_Monitor( cache_CacheId_t cacheId, cache_Monitor_t * const cacheMonitor )
{
    cacheMonitor->Hit  = 0u;
    cacheMonitor->Miss = 0u;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        cacheMonitor->Hit  = LL_ICACHE_GetHitMonitor();
        cacheMonitor->Miss = LL_ICACHE_GetMissMonitor();
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        cacheMonitor->Hit  = LL_DCACHE_Monitor_GetReadHitValue( DCACHE1 ) + LL_DCACHE_Monitor_GetWriteHitValue( DCACHE1 );
        cacheMonitor->Miss = LL_DCACHE_Monitor_GetReadMissValue( DCACHE1 ) + LL_DCACHE_Monitor_GetWriteMissValue( DCACHE1 );
    }
#endif /* DCACHE1 */
    else
    {
        /* no monitors */
    }
}


/**
 * \brief Resets the hit and miss monitors of a cache.
 *
 * \param cacheId: Cache (it exists)
 */
void Cache_Hw_Set_MonitorReset( cache_CacheId_t cacheId )
{
    if ( CACHE_INSTRUCTION == cacheId )
    {
        LL_ICACHE_ResetMonitors( LL_ICACHE_MONITOR_ALL );
    }
#if defined(DCACHE1)
    else if ( CACHE_DATA == cacheId )
    {
        LL_DCACHE_ResetMonitors( DCACHE1, LL_DCACHE_MONITOR_ALL );
    }
#endif /* DCACHE1 */
    else
    {
        /* no monitors */
    }
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Enables or disables the instruction cache.
 *
 * The disabling starts an invalidation of the cache, the bit EN reads 1 until it is finished.
 *
 * \param enabled: \ref CACHE_FLAG_ACTIVE to enable
 *
 * \return \ref CACHE_REQUEST_OK if the cache is in the required state
 */
static cache_RequestState_t Cache_Hw_Set_IcacheEnabled( cache_FlagState_t enabled )
{
    cache_RequestState_t retState   = CACHE_REQUEST_ERROR;
    cache_TimeoutCnt_t   timeoutCnt = CACHE_HW_TIMEOUT_CNT;
    uint32_t             isEnabled;

    if ( CACHE_FLAG_ACTIVE == enabled )
    {
        LL_ICACHE_Enable();

        isEnabled = LL_ICACHE_IsEnabled();

        if ( 0u != isEnabled )
        {
            retState = CACHE_REQUEST_OK;
        }
    }
    else
    {
        LL_ICACHE_ClearFlag_BSYEND();                   /* the disabling starts an invalidation */
        LL_ICACHE_Disable();

        isEnabled = LL_ICACHE_IsEnabled();

        while ( ( 0u != isEnabled ) &&
                ( 0u < timeoutCnt )    )
        {
            timeoutCnt--;
            isEnabled = LL_ICACHE_IsEnabled();
        }

        if ( 0u == isEnabled )
        {
            retState = CACHE_REQUEST_OK;
        }
    }

    return (retState);
}


/**
 * \brief Invalidates the instruction cache (when it is not busy) and waits for the end.
 *
 * \return \ref CACHE_REQUEST_OK if the invalidation is finished
 */
static cache_RequestState_t Cache_Hw_Invalidate_Icache( void )
{
    cache_RequestState_t retState   = CACHE_REQUEST_ERROR;
    cache_TimeoutCnt_t   timeoutCnt = CACHE_HW_TIMEOUT_CNT;
    uint32_t             busy       = LL_ICACHE_IsActiveFlag_BUSY();

    if ( 0u == busy )
    {
        LL_ICACHE_ClearFlag_BSYEND();
        LL_ICACHE_Invalidate();

        busy = LL_ICACHE_IsActiveFlag_BUSY();
    }

    while ( ( 0u != busy ) &&
            ( 0u < timeoutCnt )    )
    {
        timeoutCnt--;
        busy = LL_ICACHE_IsActiveFlag_BUSY();
    }

    if ( 0u == busy )
    {
        LL_ICACHE_ClearFlag_BSYEND();
        retState = CACHE_REQUEST_OK;
    }

    return (retState);
}

#if defined(DCACHE1)

/**
 * \brief Reads whether the data cache is busy (invalidation or a command on an address range).
 *
 * \return \ref CACHE_FLAG_ACTIVE if it is busy
 */
static cache_FlagState_t Cache_Hw_Get_DcacheBusy( void )
{
    cache_FlagState_t busyState = CACHE_FLAG_INACTIVE;
    uint32_t          busy      = LL_DCACHE_IsActiveFlag_BUSY( DCACHE1 );
    uint32_t          busyCmd   = LL_DCACHE_IsActiveFlag_BUSYCMD( DCACHE1 );

    if ( ( 0u != busy    ) ||
         ( 0u != busyCmd )    )
    {
        busyState = CACHE_FLAG_ACTIVE;
    }

    return (busyState);
}


/**
 * \brief Enables or disables the data cache.
 *
 * \param enabled: \ref CACHE_FLAG_ACTIVE to enable
 *
 * \return \ref CACHE_REQUEST_OK if the cache is in the required state (enabling: it was not busy)
 */
static cache_RequestState_t Cache_Hw_Set_DcacheEnabled( cache_FlagState_t enabled )
{
    cache_RequestState_t retState   = CACHE_REQUEST_ERROR;
    cache_TimeoutCnt_t   timeoutCnt = CACHE_HW_TIMEOUT_CNT;
    cache_FlagState_t    busyState;
    uint32_t             isEnabled;

    busyState = Cache_Hw_Get_DcacheBusy();

    if ( CACHE_FLAG_ACTIVE == enabled )
    {
        if ( CACHE_FLAG_ACTIVE != busyState )
        {
            LL_DCACHE_Enable( DCACHE1 );

            isEnabled = LL_DCACHE_IsEnabled( DCACHE1 );

            if ( 0u != isEnabled )
            {
                retState = CACHE_REQUEST_OK;
            }
        }
    }
    else
    {
        LL_DCACHE_Disable( DCACHE1 );

        busyState = Cache_Hw_Get_DcacheBusy();

        while ( ( CACHE_FLAG_ACTIVE == busyState ) &&
                ( 0u < timeoutCnt                 )    )
        {
            timeoutCnt--;
            busyState = Cache_Hw_Get_DcacheBusy();
        }

        isEnabled = LL_DCACHE_IsEnabled( DCACHE1 );

        if ( ( CACHE_FLAG_ACTIVE != busyState ) &&
             ( 0u == isEnabled                 )    )
        {
            retState = CACHE_REQUEST_OK;
        }
    }

    return (retState);
}


/**
 * \brief Invalidates the data cache (when it is not busy) and waits for the end.
 *
 * \return \ref CACHE_REQUEST_OK if the invalidation is finished
 */
static cache_RequestState_t Cache_Hw_Invalidate_Dcache( void )
{
    cache_RequestState_t retState   = CACHE_REQUEST_ERROR;
    cache_TimeoutCnt_t   timeoutCnt = CACHE_HW_TIMEOUT_CNT;
    cache_FlagState_t    busyState;

    busyState = Cache_Hw_Get_DcacheBusy();

    if ( CACHE_FLAG_ACTIVE != busyState )
    {
        LL_DCACHE_ClearFlag_BSYEND( DCACHE1 );
        LL_DCACHE_ClearFlag_CMDEND( DCACHE1 );
        LL_DCACHE_SetCommand( DCACHE1, 0u );            /* no operation on an address range */
        LL_DCACHE_Invalidate( DCACHE1 );

        busyState = Cache_Hw_Get_DcacheBusy();

        while ( ( CACHE_FLAG_ACTIVE == busyState ) &&
                ( 0u < timeoutCnt                 )    )
        {
            timeoutCnt--;
            busyState = Cache_Hw_Get_DcacheBusy();
        }

        if ( CACHE_FLAG_ACTIVE != busyState )
        {
            LL_DCACHE_ClearFlag_BSYEND( DCACHE1 );
            retState = CACHE_REQUEST_OK;
        }
    }

    return (retState);
}

#endif /* DCACHE1 */
