/**
 * \author Mr.Nobody
 * \file Cache.c
 * \ingroup Cache
 * \brief CPU cache (instruction and data cache) MCAL module common functionality
 *
 * Public interface of the module (the same interface as of the other families, see the README). Every cache has
 * the requested state, the suspend counter and the real state of the hardware: the cache is enabled if and only if
 * it is requested and the counter is 0. The module is event driven, nothing depends on a cyclic call of
 * \ref Cache_Task. The access to the caches is in Cache_Hw.c.
 *
 * \note The module works without \ref Cache_Init as well: the first call that needs the requested state of a cache
 *       takes over the state in which the hardware was found (the clock module enables the caches).
 *
 */
/* ============================== INCLUDES ================================== */
#include "Cache_Port.h"                     /* Own port file include          */
#include "Cache_Hw.h"                       /* Hardware level functionality   */
#include "Cache_Types.h"                    /* Module types definitions       */
#include "Stm32.h"                          /* Interrupt mask intrinsics      */
/* ============================= TYPEDEFS =================================== */

/** Context of one cache */
typedef struct
{
    cache_FlagState_t  Adopted;    /**< The requested state is known (taken over from the hardware or configured) */
    cache_FlagState_t  Requested;  /**< The application wants the cache enabled                                  */
    cache_SuspendCnt_t SuspendCnt; /**< Count of the users that need the cache suspended                         */
}   cache_Context_t;

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define CACHE_MAJOR_VERSION         ( 1u )

/** Value of minor version of SW module */
#define CACHE_MINOR_VERSION         ( 0u )

/** Value of patch version of SW module */
#define CACHE_PATCH_VERSION         ( 0u )

/* ========================== LOCAL VARIABLES =============================== */

/** Context of the caches */
static cache_Context_t cache_Context[ CACHE_ID_CNT ];

/* ======================= FORWARD DECLARATIONS ============================= */

static cache_RequestState_t Cache_Check_CacheId     ( cache_CacheId_t cacheId );
static cache_RequestState_t Cache_Check_Config      ( const cache_Config_t * const cacheConfig );
static void                 Cache_Adopt             ( cache_CacheId_t cacheId );
static cache_RequestState_t Cache_Apply             ( cache_CacheId_t cacheId );
static cache_RequestState_t Cache_Apply_Config      ( cache_CacheId_t cacheId, const cache_CacheConfig_t * const cacheConfig );
static uint32_t             Cache_Enter_Critical    ( void );
static void                 Cache_Exit_Critical     ( uint32_t interruptMask );

/* ======================== EXPORTED FUNCTIONS ============================== */

/**
 * \brief Returns the version of the module.
 *
 * \return Module SW version
 */
cache_ModuleVersion_t Cache_Get_ModuleVersion( void )
{
    cache_ModuleVersion_t retVersion;

    retVersion.Major = CACHE_MAJOR_VERSION;
    retVersion.Minor = CACHE_MINOR_VERSION;
    retVersion.Patch = CACHE_PATCH_VERSION;

    return (retVersion);
}


/**
 * \brief Applies a configuration of the caches.
 *
 * The requested state, the associativity (the instruction cache; it is changed with the cache disabled) and the
 * monitors of every cache are set. The module can be used without the initialization as well (the found state is
 * taken over), the initialization is refused while a cache is suspended.
 *
 * \param cacheConfig [in]: Pointer to the configuration structure (\ref Cache_Get_DefaultConfig keeps the found state)
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR.
 */
cache_RequestState_t Cache_Init( const cache_Config_t * const cacheConfig )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    cache_CacheId_t      cacheId;
    cache_FlagState_t    presentState;
    uint32_t             interruptMask;

    retState = Cache_Check_Config( cacheConfig );

    if ( CACHE_REQUEST_OK == retState )
    {
        interruptMask = Cache_Enter_Critical();

        for ( cacheId = (cache_CacheId_t)0u; cacheId < CACHE_ID_CNT; cacheId++ )
        {
            presentState = Cache_Hw_Get_Present( cacheId );

            if ( ( CACHE_FLAG_ACTIVE == presentState ) &&
                 ( CACHE_REQUEST_OK  == retState     )    )
            {
                retState = Cache_Apply_Config( cacheId, &cacheConfig->Cache[ cacheId ] );
            }
        }

        Cache_Exit_Critical( interruptMask );
    }

    return (retState);
}


/**
 * \brief Deinitialization of the module: forgets the requested states (the hardware is not changed).
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR (a cache is suspended).
 */
cache_RequestState_t Cache_Deinit( void )
{
    cache_RequestState_t retState = CACHE_REQUEST_OK;
    cache_CacheId_t      cacheId;
    uint32_t             interruptMask;

    interruptMask = Cache_Enter_Critical();

    for ( cacheId = (cache_CacheId_t)0u; cacheId < CACHE_ID_CNT; cacheId++ )
    {
        if ( 0u != cache_Context[ cacheId ].SuspendCnt )
        {
            retState = CACHE_REQUEST_ERROR;
        }
    }

    if ( CACHE_REQUEST_OK == retState )
    {
        for ( cacheId = (cache_CacheId_t)0u; cacheId < CACHE_ID_CNT; cacheId++ )
        {
            cache_Context[ cacheId ].Adopted    = CACHE_FLAG_INACTIVE;
            cache_Context[ cacheId ].Requested  = CACHE_FLAG_INACTIVE;
            cache_Context[ cacheId ].SuspendCnt = 0u;
        }
    }

    Cache_Exit_Critical( interruptMask );

    return (retState);
}


/**
 * \brief Common task of the MCAL modules.
 *
 * The module is event driven (a deferred enabling is done by the last \ref Cache_Set_Resume), so nothing depends on
 * this function and it does not have to be called.
 */
void Cache_Task( void )
{
    return;
}


/**
 * \brief Initialization of the configuration structure to the state found in the hardware.
 *
 * Requested state = the cache is enabled, associativity and monitors as they are. A cache the device does not have is
 * inactive.
 *
 * \param cacheConfig [out]: Pointer to the configuration structure
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR.
 */
cache_RequestState_t Cache_Get_DefaultConfig( cache_Config_t * const cacheConfig )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    cache_CacheId_t      cacheId;
    cache_FlagState_t    presentState;

    if ( CACHE_NULL_PTR != cacheConfig )
    {
        for ( cacheId = (cache_CacheId_t)0u; cacheId < CACHE_ID_CNT; cacheId++ )
        {
            cacheConfig->Cache[ cacheId ].Requested     = CACHE_FLAG_INACTIVE;
            cacheConfig->Cache[ cacheId ].Ways          = CACHE_WAYS_2;
            cacheConfig->Cache[ cacheId ].MonitorActive = CACHE_FLAG_INACTIVE;

            presentState = Cache_Hw_Get_Present( cacheId );

            if ( CACHE_FLAG_ACTIVE == presentState )
            {
                cacheConfig->Cache[ cacheId ].Requested     = Cache_Hw_Get_Enabled( cacheId );
                cacheConfig->Cache[ cacheId ].Ways          = Cache_Hw_Get_Ways( cacheId );
                cacheConfig->Cache[ cacheId ].MonitorActive = Cache_Hw_Get_MonitorActive( cacheId );
            }
        }

        retState = CACHE_REQUEST_OK;
    }

    return (retState);
}


/**
 * \brief Requests a cache to be enabled.
 *
 * The cache is enabled at once, if no user needs it suspended; otherwise the request is only remembered and it is
 * applied by the last \ref Cache_Set_Resume.
 *
 * \param cacheId: Cache
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR.
 */
cache_RequestState_t Cache_Set_Active( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    uint32_t             interruptMask;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        interruptMask = Cache_Enter_Critical();

        Cache_Adopt( cacheId );

        cache_Context[ cacheId ].Requested = CACHE_FLAG_ACTIVE;

        retState = Cache_Apply( cacheId );

        Cache_Exit_Critical( interruptMask );
    }

    return (retState);
}


/**
 * \brief Requests a cache to be disabled (applied at once).
 *
 * \param cacheId: Cache
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR.
 */
cache_RequestState_t Cache_Set_Inactive( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    uint32_t             interruptMask;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        interruptMask = Cache_Enter_Critical();

        Cache_Adopt( cacheId );

        cache_Context[ cacheId ].Requested = CACHE_FLAG_INACTIVE;

        retState = Cache_Apply( cacheId );

        Cache_Exit_Critical( interruptMask );
    }

    return (retState);
}


/**
 * \brief Reads the state of a cache.
 *
 * \param cacheId: Cache
 * \param cacheState [out]: Requested state, real state of the hardware and the suspend counter
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR.
 */
cache_RequestState_t Cache_Get_State( cache_CacheId_t cacheId, cache_State_t * const cacheState )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    uint32_t             interruptMask;

    retState = Cache_Check_CacheId( cacheId );

    if ( ( CACHE_REQUEST_OK == retState ) &&
         ( CACHE_NULL_PTR != cacheState )    )
    {
        interruptMask = Cache_Enter_Critical();

        Cache_Adopt( cacheId );

        cacheState->Requested  = cache_Context[ cacheId ].Requested;
        cacheState->Enabled    = Cache_Hw_Get_Enabled( cacheId );
        cacheState->SuspendCnt = cache_Context[ cacheId ].SuspendCnt;

        Cache_Exit_Critical( interruptMask );
    }
    else
    {
        retState = CACHE_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Suspends a cache: increments the suspend counter and disables the cache if it was enabled.
 *
 * Every \ref Cache_Set_Suspend has to be followed by a \ref Cache_Set_Resume. The calls nest. Disabling the
 * instruction cache invalidates its content.
 *
 * \param cacheId: Cache
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR (overflow of the counter, the cache did not stop).
 */
cache_RequestState_t Cache_Set_Suspend( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    uint32_t             interruptMask;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        interruptMask = Cache_Enter_Critical();

        Cache_Adopt( cacheId );

        if ( CACHE_SUSPEND_CNT_MAX > cache_Context[ cacheId ].SuspendCnt )
        {
            cache_Context[ cacheId ].SuspendCnt++;

            retState = Cache_Apply( cacheId );

            if ( CACHE_REQUEST_OK != retState )
            {
                cache_Context[ cacheId ].SuspendCnt--;
            }
        }
        else
        {
            retState = CACHE_REQUEST_ERROR;
        }

        Cache_Exit_Critical( interruptMask );
    }

    return (retState);
}


/**
 * \brief Resumes a cache: decrements the suspend counter, the last resume enables the cache if it is requested.
 *
 * \param cacheId: Cache
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR (no suspend to resume, the cache did not start).
 */
cache_RequestState_t Cache_Set_Resume( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    uint32_t             interruptMask;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        interruptMask = Cache_Enter_Critical();

        if ( 0u < cache_Context[ cacheId ].SuspendCnt )
        {
            cache_Context[ cacheId ].SuspendCnt--;

            retState = Cache_Apply( cacheId );
        }
        else
        {
            retState = CACHE_REQUEST_ERROR;
        }

        Cache_Exit_Critical( interruptMask );
    }

    return (retState);
}


/**
 * \brief Invalidates a cache and waits for the end of the invalidation.
 *
 * It can be called with the cache enabled, disabled or suspended.
 *
 * \param cacheId: Cache
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR (the cache is busy or the invalidation did not finish).
 */
cache_RequestState_t Cache_Set_Invalidate( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        retState = Cache_Hw_Invalidate( cacheId );
    }

    return (retState);
}


/**
 * \brief Cleans a range of addresses of the data cache (write-back of the dirty lines) and waits for the end.
 *
 * Only the data cache has range maintenance (the instruction cache has none, the IP has no such command for it).
 *
 * \param cacheId: Cache
 * \param address: Start address of the range (16-byte aligned)
 * \param size: Size of the range in bytes (16-byte aligned, not 0)
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR (no such cache, bad alignment, busy or timeout).
 */
cache_RequestState_t Cache_Set_CleanRange( cache_CacheId_t cacheId, uint32_t address, uint32_t size )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        retState = Cache_Hw_Set_CleanRange( cacheId, address, size );
    }
    else
    {
        /* No action required, retState already holds the error */
    }

    return (retState);
}


/**
 * \brief Invalidates a range of addresses of the data cache and waits for the end.
 *
 * Only the data cache has range maintenance (the instruction cache has none, the IP has no such command for it).
 *
 * \param cacheId: Cache
 * \param address: Start address of the range (16-byte aligned)
 * \param size: Size of the range in bytes (16-byte aligned, not 0)
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR (no such cache, bad alignment, busy or timeout).
 */
cache_RequestState_t Cache_Set_InvalidateRange( cache_CacheId_t cacheId, uint32_t address, uint32_t size )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        retState = Cache_Hw_Set_InvalidateRange( cacheId, address, size );
    }
    else
    {
        /* No action required, retState already holds the error */
    }

    return (retState);
}


/**
 * \brief Cleans and invalidates a range of addresses of the data cache and waits for the end.
 *
 * Only the data cache has range maintenance (the instruction cache has none, the IP has no such command for it).
 *
 * \param cacheId: Cache
 * \param address: Start address of the range (16-byte aligned)
 * \param size: Size of the range in bytes (16-byte aligned, not 0)
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR (no such cache, bad alignment, busy or timeout).
 */
cache_RequestState_t Cache_Set_CleanInvalidateRange( cache_CacheId_t cacheId, uint32_t address, uint32_t size )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        retState = Cache_Hw_Set_CleanInvalidateRange( cacheId, address, size );
    }
    else
    {
        /* No action required, retState already holds the error */
    }

    return (retState);
}


/**
 * \brief Reads the hit and miss monitors of a cache.
 *
 * The monitors count only when they are active (the configuration). The data cache: reads and writes together.
 *
 * \param cacheId: Cache
 * \param cacheMonitor [out]: Counters
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR.
 */
cache_RequestState_t Cache_Get_Monitor( cache_CacheId_t cacheId, cache_Monitor_t * const cacheMonitor )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    retState = Cache_Check_CacheId( cacheId );

    if ( ( CACHE_REQUEST_OK == retState ) &&
         ( CACHE_NULL_PTR != cacheMonitor )   )
    {
        Cache_Hw_Get_Monitor( cacheId, cacheMonitor );
    }
    else
    {
        retState = CACHE_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Resets the hit and miss monitors of a cache.
 *
 * \param cacheId: Cache
 *
 * \return State of request execution. Returns \ref CACHE_REQUEST_OK if request was
 *         success, otherwise returns \ref CACHE_REQUEST_ERROR.
 */
cache_RequestState_t Cache_Set_MonitorReset( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;

    retState = Cache_Check_CacheId( cacheId );

    if ( CACHE_REQUEST_OK == retState )
    {
        Cache_Hw_Set_MonitorReset( cacheId );
    }

    return (retState);
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Checks that the cache exists in the interface and in the device.
 *
 * \param cacheId: Cache
 *
 * \return \ref CACHE_REQUEST_OK if the cache can be used
 */
static cache_RequestState_t Cache_Check_CacheId( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    cache_FlagState_t    presentState;

    if ( CACHE_ID_CNT > cacheId )
    {
        presentState = Cache_Hw_Get_Present( cacheId );

        if ( CACHE_FLAG_ACTIVE == presentState )
        {
            retState = CACHE_REQUEST_OK;
        }
    }

    return (retState);
}


/**
 * \brief Checks a configuration: valid values, a cache the device does not have is not requested, no cache suspended.
 *
 * \param cacheConfig [in]: Configuration
 *
 * \return \ref CACHE_REQUEST_OK if the configuration can be applied
 */
static cache_RequestState_t Cache_Check_Config( const cache_Config_t * const cacheConfig )
{
    cache_RequestState_t retState = CACHE_REQUEST_ERROR;
    cache_CacheId_t      cacheId;
    cache_FlagState_t    presentState;

    if ( CACHE_NULL_PTR != cacheConfig )
    {
        retState = CACHE_REQUEST_OK;

        for ( cacheId = (cache_CacheId_t)0u; cacheId < CACHE_ID_CNT; cacheId++ )
        {
            if ( ( CACHE_FLAG_ACTIVE < cacheConfig->Cache[ cacheId ].Requested     ) ||
                 ( CACHE_FLAG_ACTIVE < cacheConfig->Cache[ cacheId ].MonitorActive ) ||
                 ( CACHE_WAYS_2      < cacheConfig->Cache[ cacheId ].Ways          ) ||
                 ( 0u               != cache_Context[ cacheId ].SuspendCnt         )    )
            {
                retState = CACHE_REQUEST_ERROR;
            }

            presentState = Cache_Hw_Get_Present( cacheId );

            if ( ( CACHE_FLAG_ACTIVE != presentState                            ) &&
                 ( CACHE_FLAG_ACTIVE == cacheConfig->Cache[ cacheId ].Requested )    )
            {
                retState = CACHE_REQUEST_ERROR;
            }
        }
    }

    return (retState);
}


/**
 * \brief Takes over the state of the hardware as the requested one if the requested state is not known yet.
 *
 * \param cacheId: Cache (it exists)
 */
static void Cache_Adopt( cache_CacheId_t cacheId )
{
    if ( CACHE_FLAG_ACTIVE != cache_Context[ cacheId ].Adopted )
    {
        cache_Context[ cacheId ].Requested = Cache_Hw_Get_Enabled( cacheId );
        cache_Context[ cacheId ].Adopted   = CACHE_FLAG_ACTIVE;
    }
}


/**
 * \brief Brings the hardware to the state that follows from the requested state and the suspend counter.
 *
 * \param cacheId: Cache (it exists)
 *
 * \return \ref CACHE_REQUEST_OK if the hardware is in the state
 */
static cache_RequestState_t Cache_Apply( cache_CacheId_t cacheId )
{
    cache_RequestState_t retState  = CACHE_REQUEST_OK;
    cache_FlagState_t    wantState = CACHE_FLAG_INACTIVE;
    cache_FlagState_t    hwState;

    if ( ( CACHE_FLAG_ACTIVE == cache_Context[ cacheId ].Requested  ) &&
         ( 0u                == cache_Context[ cacheId ].SuspendCnt )    )
    {
        wantState = CACHE_FLAG_ACTIVE;
    }

    hwState = Cache_Hw_Get_Enabled( cacheId );

    if ( wantState != hwState )
    {
        retState = Cache_Hw_Set_Enabled( cacheId, wantState );
    }

    return (retState);
}


/**
 * \brief Applies the configuration of one cache: associativity (cache disabled), monitors, requested state.
 *
 * \param cacheId: Cache (it exists)
 * \param cacheConfig [in]: Configuration of the cache
 *
 * \return \ref CACHE_REQUEST_OK if the cache is configured
 */
static cache_RequestState_t Cache_Apply_Config( cache_CacheId_t cacheId, const cache_CacheConfig_t * const cacheConfig )
{
    cache_RequestState_t retState = CACHE_REQUEST_OK;
    cache_FlagState_t    hwState;
    cache_Ways_t         hwWays;

    if ( CACHE_INSTRUCTION == cacheId )
    {
        hwWays = Cache_Hw_Get_Ways( cacheId );

        if ( cacheConfig->Ways != hwWays )
        {
            hwState = Cache_Hw_Get_Enabled( cacheId );

            if ( CACHE_FLAG_ACTIVE == hwState )
            {
                retState = Cache_Hw_Set_Enabled( cacheId, CACHE_FLAG_INACTIVE );
            }

            Cache_Hw_Set_Ways( cacheId, cacheConfig->Ways );
        }
    }

    Cache_Hw_Set_MonitorActive( cacheId, cacheConfig->MonitorActive );

    cache_Context[ cacheId ].Requested = cacheConfig->Requested;
    cache_Context[ cacheId ].Adopted   = CACHE_FLAG_ACTIVE;

    if ( CACHE_REQUEST_OK == retState )
    {
        retState = Cache_Apply( cacheId );
    }

    return (retState);
}


/**
 * \brief Starts a critical section (the interrupts are masked).
 *
 * \return Previous state of the interrupt mask for \ref Cache_Exit_Critical
 */
static uint32_t Cache_Enter_Critical( void )
{
    uint32_t interruptMask = __get_PRIMASK();

    __disable_irq();

    return (interruptMask);
}


/**
 * \brief Ends a critical section (restores the interrupt mask).
 *
 * \param interruptMask: Value returned by \ref Cache_Enter_Critical
 */
static void Cache_Exit_Critical( uint32_t interruptMask )
{
    __set_PRIMASK( interruptMask );
}
