/**
 * \author Mr.Nobody
 * \file ItTest_Cache.c
 * \ingroup Cache
 * \brief Integration tests of the CPU cache (instruction and data cache) module on target.
 *
 * Cache module runs on the target with the real Rcc and Nvic modules (no mocks). Every test case runs after
 * system reset of the target: the caches are in the state in which the Rcc module leaves them. Target specific
 * code (entry point, initialization, fault handlers) is in ItTarget_Cache.c.
 *
 * The reason of the module (Bug AB#1176): with the instruction cache enabled a CPU read of the system data area
 * (OTP, unique ID, flash size register, 0x08FFF000 - 0x08FFFFFF) raises a precise bus fault. The tests read the flash
 * size register and the unique ID with the cache suspended - a fault would be reported by the fault handler of the
 * target as a failure. The read without the suspend is not tested (it is the fault). The OTP area is not read: its
 * blank words raise an NMI (Bug AB#1189), the OTP access belongs to the tests of the Flash module.
 *
 * The data cache exists only on the lines that have the DCACHE1 (not STM32H503): the test of the data cache expects
 * the result according to the device.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Cache_Port.h"                     /* Module under test              */
#include "Stm32.h"                          /* STM32 registers definition     */
#include "Stm32_icache.h"                   /* ICACHE registers               */
#include "Stm32_dcache.h"                   /* DCACHE registers               */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Address of the flash size register (system data area, read as 16 bits, in Kbytes) */
#define IT_CACHE_FLASH_SIZE_ADDRESS         ( 0x08FFF80Cu )

/** Start of the unique ID (96 bits, system data area, programmed in production) */
#define IT_CACHE_UID_BASE                   ( 0x08FFF800u )

/** Count of the 32-bit words of the unique ID */
#define IT_CACHE_UID_WORD_CNT               ( 3u )

/** Maximal flash size of the STM32H5 devices [Kbytes] */
#define IT_CACHE_FLASH_SIZE_MAX             ( 4096u )

/** Count of the calls of the code executed to load the instruction cache */
#define IT_CACHE_CODE_LOOP_CNT              ( 200u )

/** Size of the buffer used by the range maintenance tests (4 data cache lines) */
#define IT_CACHE_RANGE_BUFFER_SIZE          ( 64u )

/** Alignment of the buffer used by the range maintenance tests (the data cache line size) */
#define IT_CACHE_RANGE_ALIGN_BYTES          ( 16u )

/* ========================= EXPORTED VARIABLES ============================= */

/** Buffer used as the address range of the maintenance commands (aligned to the data cache line) */
static uint8_t itCache_RangeBuffer[ IT_CACHE_RANGE_BUFFER_SIZE ]
    __attribute__(( aligned( IT_CACHE_RANGE_ALIGN_BYTES ) ));

/* ======================= FORWARD DECLARATIONS ============================= */

static uint32_t itCache_Get_HwEnabled   ( void );
static uint32_t itCache_Read_SystemData ( void );

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
}

void tearDown( void )
{
}

/* ============================== TESTS ===================================== */

/** After the boot the module reports the state in which the Rcc module left the instruction cache. */
void It_Cache_Get_State_AfterBoot_ReportsHardwareState( void )
{
    cache_State_t state;
    uint32_t      hwEnabled = itCache_Get_HwEnabled();

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );

    TEST_ASSERT_EQUAL_UINT8( 0u, state.SuspendCnt );
    TEST_ASSERT_EQUAL_UINT32( hwEnabled, state.Requested );
    TEST_ASSERT_EQUAL_UINT32( hwEnabled, state.Enabled );
}


/** The system data area (flash size register, unique ID) is readable with the instruction cache suspended (Bug AB#1176). */
void It_Cache_SetSuspend_ReadOfSystemDataArea_NoBusFault( void )
{
    cache_State_t state;
    uint32_t      flashSize;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Active( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCache_Get_HwEnabled() );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCache_Get_HwEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL_UINT8( 1u, state.SuspendCnt );

    flashSize = itCache_Read_SystemData();                              /* a bus fault is reported by the target */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCache_Get_HwEnabled() );

    TEST_ASSERT_GREATER_THAN_UINT32( 0u, flashSize );
    TEST_ASSERT_LESS_OR_EQUAL_UINT32( IT_CACHE_FLASH_SIZE_MAX, flashSize );
}


/** Nested suspends: the instruction cache is enabled again by the last resume only. */
void It_Cache_SetSuspend_Nested_EnabledByTheLastResume( void )
{
    cache_State_t state;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Active( CACHE_INSTRUCTION ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL_UINT8( 2u, state.SuspendCnt );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCache_Get_HwEnabled() );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCache_Get_HwEnabled() );            /* the other user still needs it off */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCache_Get_HwEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, state.SuspendCnt );
}


/** A resume without a suspend is refused and changes nothing. */
void It_Cache_SetResume_WithoutSuspend_ReturnErrorStateUnchanged( void )
{
    uint32_t hwEnabled = itCache_Get_HwEnabled();

    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( hwEnabled, itCache_Get_HwEnabled() );
}


/** The request to disable and to enable the instruction cache is applied to the hardware. */
void It_Cache_SetInactiveAndActive_HardwareFollowsTheRequest( void )
{
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Inactive( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCache_Get_HwEnabled() );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Active( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCache_Get_HwEnabled() );
}


/** A request to enable made during a suspend is applied by the resume. */
void It_Cache_SetActive_DuringSuspend_AppliedByResume( void )
{
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Inactive( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Active( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCache_Get_HwEnabled() );            /* only remembered */

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCache_Get_HwEnabled() );
}


/** The invalidation finishes with the cache enabled, suspended and disabled; it does not change the state. */
void It_Cache_SetInvalidate_EnabledAndDisabled_ReturnOkStateUnchanged( void )
{
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Active( CACHE_INSTRUCTION ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Invalidate( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCache_Get_HwEnabled() );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Invalidate( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCache_Get_HwEnabled() );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_INSTRUCTION ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Inactive( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Invalidate( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCache_Get_HwEnabled() );
}


/** The monitors count the accesses of the code executed with the instruction cache enabled. */
void It_Cache_Get_Monitor_CodeExecution_CountersCount( void )
{
    cache_Config_t  config;
    cache_Monitor_t monitor;
    cache_State_t   state;
    uint32_t        loopIdx;
    uint32_t        hitCnt;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_DefaultConfig( &config ) );
    config.Cache[ CACHE_INSTRUCTION ].Requested     = CACHE_FLAG_ACTIVE;
    config.Cache[ CACHE_INSTRUCTION ].MonitorActive = CACHE_FLAG_ACTIVE;
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Init( &config ) );

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_MonitorReset( CACHE_INSTRUCTION ) );

    for ( loopIdx = 0u; loopIdx < IT_CACHE_CODE_LOOP_CNT; loopIdx++ )
    {
        TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_INSTRUCTION, &state ) );   /* the same code again and again */
    }

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_Monitor( CACHE_INSTRUCTION, &monitor ) );
    TEST_ASSERT_GREATER_THAN_UINT32( 0u, monitor.Hit );
    hitCnt = monitor.Hit;

    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_MonitorReset( CACHE_INSTRUCTION ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_Monitor( CACHE_INSTRUCTION, &monitor ) );
    TEST_ASSERT_LESS_THAN_UINT32( hitCnt, monitor.Hit );                /* the counters restarted */
}


/** The data cache is used where the device has it, the other devices return an error. */
void It_Cache_DataCache_PresentOrAbsent_ResultAccordingToTheDevice( void )
{
    cache_State_t state;

#if defined(DCACHE1)
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Get_State( CACHE_DATA, &state ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Suspend( CACHE_DATA ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, LL_DCACHE_IsEnabled( DCACHE1 ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Resume( CACHE_DATA ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK, Cache_Set_Invalidate( CACHE_DATA ) );
#else
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Get_State( CACHE_DATA, &state ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Suspend( CACHE_DATA ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR, Cache_Set_Invalidate( CACHE_DATA ) );
#endif
}


/** The range maintenance commands run on a real buffer where the device has the data cache, the other devices
    return an error (compile-only, not run on hardware). */
void It_Cache_RangeMaintenance_PresentOrAbsent_ResultAccordingToTheDevice( void )
{
    uint32_t bufferAddress = (uint32_t)(uintptr_t)itCache_RangeBuffer;

#if defined(DCACHE1)
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK,
                        Cache_Set_CleanRange( CACHE_DATA, bufferAddress, IT_CACHE_RANGE_BUFFER_SIZE ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK,
                        Cache_Set_InvalidateRange( CACHE_DATA, bufferAddress, IT_CACHE_RANGE_BUFFER_SIZE ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_OK,
                        Cache_Set_CleanInvalidateRange( CACHE_DATA, bufferAddress, IT_CACHE_RANGE_BUFFER_SIZE ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR,
                        Cache_Set_CleanRange( CACHE_DATA, bufferAddress + 1u, IT_CACHE_RANGE_BUFFER_SIZE ) );
#else
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR,
                        Cache_Set_CleanRange( CACHE_DATA, bufferAddress, IT_CACHE_RANGE_BUFFER_SIZE ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR,
                        Cache_Set_InvalidateRange( CACHE_DATA, bufferAddress, IT_CACHE_RANGE_BUFFER_SIZE ) );
    TEST_ASSERT_EQUAL( CACHE_REQUEST_ERROR,
                        Cache_Set_CleanInvalidateRange( CACHE_DATA, bufferAddress, IT_CACHE_RANGE_BUFFER_SIZE ) );
#endif
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Reads the enable bit of the instruction cache directly from the register.
 *
 * \return 1 if the instruction cache is enabled
 */
static uint32_t itCache_Get_HwEnabled( void )
{
    return (LL_ICACHE_IsEnabled());
}


/**
 * \brief Reads the unique ID and the flash size register (the system data area of the device).
 *
 * \return Flash size [Kbytes]
 */
static uint32_t itCache_Read_SystemData( void )
{
    const volatile uint16_t * const flashSize = (const volatile uint16_t *)IT_CACHE_FLASH_SIZE_ADDRESS;
    const volatile uint32_t * const uid       = (const volatile uint32_t *)IT_CACHE_UID_BASE;
    uint32_t                        wordIdx;
    uint32_t                        sum = 0u;

    for ( wordIdx = 0u; wordIdx < IT_CACHE_UID_WORD_CNT; wordIdx++ )
    {
        sum += uid[ wordIdx ];
    }

    (void)sum;                                                          /* the words are only read (value irrelevant) */

    return ((uint32_t)*flashSize);
}
