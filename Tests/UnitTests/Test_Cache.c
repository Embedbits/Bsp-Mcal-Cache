/**
 * \author Mr.Nobody
 * \file Test_Cache.c
 * \ingroup Cache
 * \brief Unit tests of the CPU cache (instruction and data cache) module.
 *
 * All Cache sources are compiled unchanged with the real LL drivers. The registers of the ICACHE and of the DCACHE1
 * are emulated by RegMem (plain memory): the tests preset the flags the hardware would set and check the bits the
 * module writes. The enable bit of the caches follows the writes (plain memory), the invalidation request bit stays
 * set (the hardware clears it).
 *
 * The lines without the DCACHE1 (STM32H503) test that the data cache functions return an error.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Cache_Port.h"                     /* Module under test              */
#include "Cache_Hw.h"                       /* Hardware layer of the module   */
#include "Stm32.h"                          /* STM32 registers definition     */
#include "Stm32_icache.h"                   /* ICACHE registers               */
#include "Stm32_dcache.h"                   /* DCACHE registers               */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ======================= FORWARD DECLARATIONS ============================= */

static void Ut_Cache_Preset_Icache  ( uint32_t enabled, uint32_t ways2 );

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    cache_CacheId_t cacheId;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    for ( cacheId = (cache_CacheId_t)0u; cacheId < CACHE_ID_CNT; cacheId++ )
    {
        while ( CACHE_REQUEST_OK == Cache_Set_Resume( cacheId ) )
        {
            /* the context of the module is static: all suspends of the previous test are released */
        }
    }

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Deinit() );
}

void tearDown( void )
{
}

/* ============================ MODULE VERSION ============================== */

void Ut_Cache_Get_ModuleVersion_ReturnVersion( void )
{
    cache_ModuleVersion_t version = Cache_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Minor );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Patch );
}

/* ========================= DEFAULT CONFIGURATION ========================== */

void Ut_Cache_Get_DefaultConfig_FoundStateOfInstructionCache( void )
{
    cache_Config_t config;

    Ut_Cache_Preset_Icache( 1u, 0u );                                   /* enabled, 1-way */
    LL_ICACHE_EnableMonitors( LL_ICACHE_MONITOR_ALL );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, config.Cache[ CACHE_INSTRUCTION ].Requested );
    TEST_ASSERT_EQUAL( CACHE_WAYS_1, config.Cache[ CACHE_INSTRUCTION ].Ways );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, config.Cache[ CACHE_INSTRUCTION ].MonitorActive );

    Ut_Cache_Preset_Icache( 0u, 1u );                                   /* disabled, 2-ways */
    LL_ICACHE_DisableMonitors( LL_ICACHE_MONITOR_ALL );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, config.Cache[ CACHE_INSTRUCTION ].Requested );
    TEST_ASSERT_EQUAL( CACHE_WAYS_2, config.Cache[ CACHE_INSTRUCTION ].Ways );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, config.Cache[ CACHE_INSTRUCTION ].MonitorActive );
}

void Ut_Cache_Get_DefaultConfig_DataCache_FoundStateOrAbsent( void )
{
    cache_Config_t config;

#if defined(DCACHE1)
    LL_DCACHE_Enable( DCACHE1 );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, config.Cache[ CACHE_DATA ].Requested );
#else
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, config.Cache[ CACHE_DATA ].Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, config.Cache[ CACHE_DATA ].MonitorActive );
#endif
}

void Ut_Cache_Get_DefaultConfig_NullPointer_ReturnError( void )
{
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Get_DefaultConfig( CACHE_NULL_PTR ) );
}

/* ============================ INITIALIZATION ============================== */

void Ut_Cache_Init_Configuration_AppliedToInstructionCache( void )
{
    cache_Config_t config;
    cache_State_t  state;

    Ut_Cache_Preset_Icache( 1u, 1u );                                   /* enabled, 2-ways */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_INSTRUCTION ].Ways          = CACHE_WAYS_1;
    config.Cache[ CACHE_INSTRUCTION ].MonitorActive = CACHE_FLAG_ACTIVE;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( LL_ICACHE_1WAY, LL_ICACHE_GetMode() );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );              /* enabled again after the change of the ways */
    TEST_ASSERT_EQUAL_UINT32( LL_ICACHE_MONITOR_ALL, ICACHE->CR & LL_ICACHE_MONITOR_ALL );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Enabled );
}

void Ut_Cache_Init_TwoWays_WaysChangedWithCacheDisabled( void )
{
    cache_Config_t config;

    Ut_Cache_Preset_Icache( 1u, 0u );                                   /* enabled, 1-way */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_INSTRUCTION ].Ways = CACHE_WAYS_2;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( LL_ICACHE_2WAYS, LL_ICACHE_GetMode() );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
}

void Ut_Cache_Init_DataCacheConfiguration_AppliedOrAbsent( void )
{
#if defined(DCACHE1)
    cache_Config_t config;
    cache_State_t  state;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_DATA ].Requested     = CACHE_FLAG_ACTIVE;
    config.Cache[ CACHE_DATA ].MonitorActive = CACHE_FLAG_ACTIVE;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( 1u, LL_DCACHE_IsEnabled( DCACHE1 ) );
    TEST_ASSERT_EQUAL_UINT32( LL_DCACHE_MONITOR_ALL, DCACHE1->CR & LL_DCACHE_MONITOR_ALL );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_DATA, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Enabled );

    config.Cache[ CACHE_DATA ].Requested     = CACHE_FLAG_INACTIVE;
    config.Cache[ CACHE_DATA ].MonitorActive = CACHE_FLAG_INACTIVE;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( 0u, LL_DCACHE_IsEnabled( DCACHE1 ) );
    TEST_ASSERT_BITS_LOW( LL_DCACHE_MONITOR_ALL, DCACHE1->CR );
#else
    TEST_IGNORE_MESSAGE( "The device has no data cache" );
#endif
}

void Ut_Cache_Init_RequestedInactive_CacheDisabled( void )
{
    cache_Config_t config;
    cache_State_t  state;

    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_INSTRUCTION ].Requested = CACHE_FLAG_INACTIVE;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( 0u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Enabled );
}

void Ut_Cache_Init_InvalidConfig_ReturnError( void )
{
    cache_Config_t config;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Init( CACHE_NULL_PTR ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_INSTRUCTION ].Ways = (cache_Ways_t)( CACHE_WAYS_2 + 1u );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Init( &config ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_INSTRUCTION ].Requested = (cache_FlagState_t)( CACHE_FLAG_ACTIVE + 1u );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Init( &config ) );

#if !defined(DCACHE1)
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_DATA ].Requested = CACHE_FLAG_ACTIVE;           /* the device has no data cache */
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Init( &config ) );
#endif
}

void Ut_Cache_Init_CacheSuspended_ReturnError( void )
{
    cache_Config_t config;

    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Init( &config ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Deinit() );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Deinit() );
}

/* ============================ REQUESTED STATE ============================= */

void Ut_Cache_Get_State_WithoutInit_AdoptsFoundState( void )
{
    cache_State_t state;

    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Enabled );
    TEST_ASSERT_EQUAL_UINT8( 0u, state.SuspendCnt );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Deinit() );              /* the next call adopts the hardware state again */
    Ut_Cache_Preset_Icache( 0u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Enabled );
}

void Ut_Cache_SetActiveAndInactive_CacheFollowsTheRequest( void )
{
    cache_State_t state;

    Ut_Cache_Preset_Icache( 0u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Active( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Requested );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Inactive( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Enabled );
}

void Ut_Cache_DisablingInstructionCache_InvalidationStartedByHardware( void )
{
    Ut_Cache_Preset_Icache( 1u, 1u );
    ICACHE->FCR = 0u;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Inactive( CACHE_INSTRUCTION ) );

    TEST_ASSERT_BITS_HIGH( ICACHE_FCR_CBSYENDF, ICACHE->FCR );          /* BSYENDF cleared before the disabling (as the HAL does) */
}

void Ut_Cache_InvalidIdOrNullPointer_ReturnError( void )
{
    cache_State_t   state;
    cache_Monitor_t monitor;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Active( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Inactive( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Get_State( CACHE_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Get_State( CACHE_INSTRUCTION, CACHE_NULL_PTR ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Suspend( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Resume( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Invalidate( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Get_Monitor( CACHE_ID_CNT, &monitor ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Get_Monitor( CACHE_INSTRUCTION, CACHE_NULL_PTR ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_MonitorReset( CACHE_ID_CNT ) );
}

/* ============================ SUSPEND AND RESUME ========================== */

void Ut_Cache_SetSuspendAndResume_CacheOffUntilTheLastResume( void )
{
    cache_State_t state;

    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Enabled );
    TEST_ASSERT_EQUAL_UINT8( 1u, state.SuspendCnt );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );      /* another user */
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL_UINT8( 2u, state.SuspendCnt );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, LL_ICACHE_IsEnabled() );                              /* the first user still needs it off */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Enabled );
    TEST_ASSERT_EQUAL_UINT8( 0u, state.SuspendCnt );
}

void Ut_Cache_SetResume_WithoutSuspend_ReturnErrorNothingChanged( void )
{
    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
}

void Ut_Cache_SetActiveDuringSuspend_RememberedAndAppliedByTheLastResume( void )
{
    cache_State_t state;

    Ut_Cache_Preset_Icache( 0u, 1u );                                                   /* disabled when found */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Active( CACHE_INSTRUCTION ) );      /* e.g. from an interrupt */

    TEST_ASSERT_EQUAL_UINT32( 0u, LL_ICACHE_IsEnabled() );                              /* only remembered */
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Enabled );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );                              /* applied by the last resume */
}

void Ut_Cache_SetInactiveDuringSuspend_CacheStaysOffAfterResume( void )
{
    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Inactive( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );

    TEST_ASSERT_EQUAL_UINT32( 0u, LL_ICACHE_IsEnabled() );
}

void Ut_Cache_SuspendOfDisabledCache_ResumeDoesNotEnableIt( void )
{
    Ut_Cache_Preset_Icache( 0u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );

    TEST_ASSERT_EQUAL_UINT32( 0u, LL_ICACHE_IsEnabled() );
}

void Ut_Cache_SetSuspend_CounterOverflow_ReturnErrorCounterUnchanged( void )
{
    cache_State_t state;
    uint32_t      suspendId;

    Ut_Cache_Preset_Icache( 1u, 1u );

    for ( suspendId = 0u; suspendId < CACHE_SUSPEND_CNT_MAX; suspendId++ )
    {
        TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    }

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL_UINT8( CACHE_SUSPEND_CNT_MAX, state.SuspendCnt );

    for ( suspendId = 0u; suspendId < CACHE_SUSPEND_CNT_MAX; suspendId++ )
    {
        TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    }

    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
}

void Ut_Cache_SetSuspend_DataCache_FollowsTheSameRulesOrIsAbsent( void )
{
#if defined(DCACHE1)
    cache_State_t state;

    LL_DCACHE_Enable( DCACHE1 );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_DATA ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, LL_DCACHE_IsEnabled( DCACHE1 ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_DATA, &state ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_ACTIVE, state.Requested );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Enabled );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_DATA ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_DCACHE_IsEnabled( DCACHE1 ) );
#else
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Suspend( CACHE_DATA ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Resume( CACHE_DATA ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Active( CACHE_DATA ) );
#endif
}

void Ut_Cache_SetActive_DataCacheBusy_ReturnError( void )
{
#if defined(DCACHE1)
    LL_DCACHE_Disable( DCACHE1 );
    DCACHE1->SR = DCACHE_SR_BUSYF;                                                      /* an invalidation runs */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Active( CACHE_DATA ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, LL_DCACHE_IsEnabled( DCACHE1 ) );
#else
    TEST_IGNORE_MESSAGE( "The device has no data cache" );
#endif
}

void Ut_Cache_SetSuspend_DataCacheBusyForever_ReturnErrorCounterUnchanged( void )
{
#if defined(DCACHE1)
    cache_State_t state;

    LL_DCACHE_Enable( DCACHE1 );
    DCACHE1->SR = DCACHE_SR_BUSYF;                                                      /* the disabling never ends */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Suspend( CACHE_DATA ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_DATA, &state ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, state.SuspendCnt );
#else
    TEST_IGNORE_MESSAGE( "The device has no data cache" );
#endif
}

/* ================================ INVALIDATION ============================ */

void Ut_Cache_SetInvalidate_InstructionCache_InvalidationRequested( void )
{
    Ut_Cache_Preset_Icache( 1u, 1u );
    ICACHE->SR = ICACHE_SR_BSYENDF;                                                     /* a stale flag */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Invalidate( CACHE_INSTRUCTION ) );

    TEST_ASSERT_BITS_HIGH( ICACHE_CR_CACHEINV, ICACHE->CR );
    TEST_ASSERT_BITS_HIGH( ICACHE_FCR_CBSYENDF, ICACHE->FCR );                          /* the end flag is cleared */
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );                              /* the state is not changed */
}

void Ut_Cache_SetInvalidate_CacheDisabledOrSuspended_InvalidationRequested( void )
{
    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    ICACHE->CR &= ~ICACHE_CR_CACHEINV;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Invalidate( CACHE_INSTRUCTION ) );
    TEST_ASSERT_BITS_HIGH( ICACHE_CR_CACHEINV, ICACHE->CR );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
}

void Ut_Cache_SetInvalidate_InstructionCacheBusyForever_ReturnErrorTimeout( void )
{
    Ut_Cache_Preset_Icache( 1u, 1u );
    ICACHE->SR = ICACHE_SR_BUSYF;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Invalidate( CACHE_INSTRUCTION ) );
    TEST_ASSERT_BITS_LOW( ICACHE_CR_CACHEINV, ICACHE->CR );                             /* not started while busy */
}

void Ut_Cache_SetInvalidate_DataCache_InvalidationRequestedOrAbsent( void )
{
#if defined(DCACHE1)
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Invalidate( CACHE_DATA ) );
    TEST_ASSERT_BITS_HIGH( DCACHE_CR_CACHEINV, DCACHE1->CR );
    TEST_ASSERT_BITS_LOW( DCACHE_CR_CACHECMD, DCACHE1->CR );                            /* no command on an address range */

    DCACHE1->SR = DCACHE_SR_BUSYF;
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Invalidate( CACHE_DATA ) );       /* busy: refused */
#else
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Invalidate( CACHE_DATA ) );
#endif
}

/* ================================ MONITORS ================================ */

void Ut_Cache_GetMonitor_InstructionCache_HitAndMissCounters( void )
{
    cache_Monitor_t monitor;

    ICACHE->HMONR = 1234u;
    ICACHE->MMONR = 56u;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_Monitor( CACHE_INSTRUCTION, &monitor ) );
    TEST_ASSERT_EQUAL_UINT32( 1234u, monitor.Hit );
    TEST_ASSERT_EQUAL_UINT32( 56u, monitor.Miss );
}

/* The reset of the counters is a pulse (set and release of the reset bits by the LL): plain memory cannot show
   the pulse, the test checks that the reset bits are released and the monitors are not disturbed. */
void Ut_Cache_SetMonitorReset_InstructionCache_ResetReleasedMonitorsKeptActive( void )
{
    LL_ICACHE_EnableMonitors( LL_ICACHE_MONITOR_ALL );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_MonitorReset( CACHE_INSTRUCTION ) );

    TEST_ASSERT_BITS_LOW( ICACHE_CR_HITMRST | ICACHE_CR_MISSMRST, ICACHE->CR );
    TEST_ASSERT_BITS_HIGH( ICACHE_CR_HITMEN | ICACHE_CR_MISSMEN, ICACHE->CR );
}

void Ut_Cache_Monitor_DataCache_SumOfReadsAndWritesOrAbsent( void )
{
#if defined(DCACHE1)
    cache_Monitor_t monitor;

    DCACHE1->RHMONR = 100u;
    DCACHE1->RMMONR = 10u;
    DCACHE1->WHMONR = 20u;
    DCACHE1->WMMONR = 5u;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_Monitor( CACHE_DATA, &monitor ) );
    TEST_ASSERT_EQUAL_UINT32( 120u, monitor.Hit );
    TEST_ASSERT_EQUAL_UINT32( 15u, monitor.Miss );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_MonitorReset( CACHE_DATA ) );
    TEST_ASSERT_BITS_LOW( DCACHE_CR_RHITMRST | DCACHE_CR_RMISSMRST | DCACHE_CR_WHITMRST | DCACHE_CR_WMISSMRST, DCACHE1->CR );
#else
    cache_Monitor_t monitor;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Get_Monitor( CACHE_DATA, &monitor ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_MonitorReset( CACHE_DATA ) );
#endif
}

/* ======================== HARDWARE LAYER FALLBACKS ======================== */

/* The module checks the cache before it calls the hardware layer: the fallbacks are tested on the layer itself */
void Ut_Cache_Hw_UnknownCache_ReturnsInactiveOrError( void )
{
    cache_Monitor_t monitor = { 1u, 2u };

    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, Cache_Hw_Get_Present( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, Cache_Hw_Get_Enabled( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Hw_Set_Enabled( CACHE_ID_CNT, CACHE_FLAG_ACTIVE ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Hw_Invalidate( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, Cache_Hw_Get_MonitorActive( CACHE_ID_CNT ) );
    TEST_ASSERT_EQUAL( CACHE_WAYS_2, Cache_Hw_Get_Ways( CACHE_ID_CNT ) );

    Cache_Hw_Set_Ways( CACHE_ID_CNT, CACHE_WAYS_1 );                    /* nothing to set */
    Cache_Hw_Set_MonitorActive( CACHE_ID_CNT, CACHE_FLAG_ACTIVE );
    Cache_Hw_Set_MonitorReset( CACHE_ID_CNT );
    Cache_Hw_Get_Monitor( CACHE_ID_CNT, &monitor );

    TEST_ASSERT_EQUAL_UINT32( 0u, monitor.Hit );
    TEST_ASSERT_EQUAL_UINT32( 0u, monitor.Miss );
    TEST_ASSERT_EQUAL_UINT32( LL_ICACHE_1WAY, LL_ICACHE_GetMode() );                   /* unchanged by the unknown cache */
}

/* ============================ TASK AND DEINIT ============================= */

void Ut_Cache_Task_NothingToServe_NoEffect( void )
{
    cache_State_t state;

    Ut_Cache_Preset_Icache( 1u, 1u );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );

    Cache_Task();
    Cache_Task();

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL_UINT8( 1u, state.SuspendCnt );
    TEST_ASSERT_EQUAL( CACHE_FLAG_INACTIVE, state.Enabled );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );       /* the resume does not need the task */
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
}

/* ========================== LOCAL FUNCTIONS =============================== */

/** Presets the state of the instruction cache in the emulated registers (enable bit and associativity) */
static void Ut_Cache_Preset_Icache( uint32_t enabled, uint32_t ways2 )
{
    ICACHE->CR = 0u;

    if ( 0u != enabled )
    {
        LL_ICACHE_Enable();
    }

    if ( 0u != ways2 )
    {
        LL_ICACHE_SetMode( LL_ICACHE_2WAYS );
    }
    else
    {
        LL_ICACHE_SetMode( LL_ICACHE_1WAY );
    }
}
