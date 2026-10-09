# Cache Peripheral Driver

This module provides an abstraction layer for the **CPU caches** of STM32 MCUs - the instruction cache and the data
cache: enable and disable, **suspend and resume** (a temporary bypass of the cache that is safe against other users of
the module), invalidation and the hit / miss monitors. It is a low-level MCAL module: it owns the cache hardware, the
other modules (flash, memories, middlewares) ask it for what they need and do not touch the cache registers.

The public interface (`Cache_Port.h`, `Cache_Types.h`) is **identical for all supported families**. The implementation
for a family lives in its own branch (`Dev/<Family>` for development, `Releases/<Family>` for the released versions), so
a module or an application written against the interface does not change when the MCU family changes.

This README is the landing page and the **specification of the interface** (master). The family branches contain their
own README with everything that is specific to the family: the caches of the device, their settings and the status of
the implementation.

> **Status: design.** No implementation exists yet, the interface below is a proposal. Stories and branches follow.

---

## Intent and purpose

- Every STM32 family has a cache between the CPU and the flash (the instruction cache of STM32H5 / U5, the ART
  accelerator and the caches of the flash interface of STM32F4 / G4 / L4, the L1 caches of the Cortex-M7 of STM32F7 /
  H7). Today the cache is switched on by the clock module as a side effect of the clock initialization, nobody owns it
  and nobody can change it at run time.
- **Some memory must not be read through the cache.** On STM32H5 the OTP area, the unique ID, the flash size register
  and the high-cycle data flash are in the cacheable address range; a read of them with the instruction cache enabled
  ends with a precise bus fault (a non-cacheable MPU region or a read with the cache disabled is the cure). A module
  that has to read such memory (the flash module does) needs to **disable the cache for a short time and enable it
  again** - without switching the cache on behind the back of another module that needs it off, and without leaving it
  off when it was on before.
- **A cache must be invalidated when the memory behind it changes.** After an erase or a write of the flash the cache can
  return the old content (on STM32F4 the data cache of the ART accelerator has to be reset after an erase). The module
  offers the invalidation for every family under the same name.
- The module hides the differences between the families: the application and the other modules call
  `Cache_Set_Suspend` / `Cache_Set_Resume` / `Cache_Set_Invalidate` and do not care whether there is an ICACHE, an ART
  accelerator or an L1 cache.

What the module is **not**: it does not configure the MPU (the attributes of the memory regions) and it does not change
the flash latency or the prefetch (RCC module). It does not do the maintenance of the cache by address (clean /
invalidate of a range for DMA buffers of the Cortex-M7) in the first version.

---

## Concept

A family can have more caches, every one of them is identified by `cache_CacheId_t`:

| Cache | Meaning | Families (planned) |
|---|---|---|
| `CACHE_INSTRUCTION` | Cache of the code (and of the constants) read from the flash | all |
| `CACHE_DATA` | Cache of the data (external memories, Cortex-M7 data cache) | where the device has one |

For every cache the module keeps two states and a counter:

| Item | Meaning |
|---|---|
| **Requested** | What the application wants: `Cache_Set_Active` / `Cache_Set_Inactive`. After `Cache_Init` it is the state in which the cache was found. |
| **Suspend counter** | How many users need the cache off right now (`Cache_Set_Suspend` increments, `Cache_Set_Resume` decrements). |
| **Enabled** | The real state of the hardware: enabled if and only if it is requested **and** the suspend counter is 0. |

Rules:

1. `Cache_Set_Suspend` increments the counter and disables the cache if it was enabled.
2. `Cache_Set_Resume` decrements the counter; when it reaches 0 and the cache is requested, it is enabled again (in the
   same call). `Cache_Set_Resume` without `Cache_Set_Suspend` returns an error.
3. A request to enable the cache (`Cache_Set_Active`) during a suspend is **only remembered** - it is applied by the
   last `Cache_Set_Resume`. A request to disable (`Cache_Set_Inactive`) is applied at once.
4. The suspend, the resume and the requests are executed in a critical section (a single core), so they can be called
   from an interrupt as well. Nothing waits for a process to finish and nothing depends on a cyclic call: the module is
   event driven, `Cache_Task()` is not needed for the correctness (see below).
5. `Cache_Set_Invalidate` invalidates the content of the cache (it waits for the end of the invalidation of the
   hardware). It can be called with the cache enabled, disabled or suspended.

---

## Supported families

Every family gets its own story, its own tasks and its own branches `Dev/<Family>` and `Releases/<Family>` cut from this
master.

| Family | Branch | Caches | Status |
|---|---|---|---|
| STM32H5 | `Dev/STM32H5` | instruction cache (ICACHE), data cache (DCACHE1 on the lines that have it) | planned (first) |
| STM32U5 | `Dev/STM32U5` | ICACHE, DCACHE | planned |
| STM32F4 | `Dev/STM32F4` | instruction and data cache of the ART accelerator (`FLASH_ACR`) | planned |
| STM32G4, STM32L4 | `Dev/STM32G4`, `Dev/STM32L4` | instruction and data cache of the flash interface (`FLASH_ACR`) | planned |
| STM32F7, STM32H7 | `Dev/STM32F7`, `Dev/STM32H7` | L1 instruction and data cache of the Cortex-M7 (SCB), ART accelerator (F7) | planned |

A function without a meaning in a family stays in the interface, returns `CACHE_REQUEST_ERROR` and suppresses the unused
parameters (for example a cache the device does not have, or the remap regions).

---

## Public interface in short (proposal)

| Group | Functions |
|---|---|
| Module management | `Cache_Get_ModuleVersion`, `Cache_Init`, `Cache_Deinit`, `Cache_Task`, `Cache_Get_DefaultConfig` |
| Requested state | `Cache_Set_Active`, `Cache_Set_Inactive`, `Cache_Get_State` |
| Temporary bypass | `Cache_Set_Suspend`, `Cache_Set_Resume` |
| Maintenance | `Cache_Set_Invalidate` |
| Monitors | `Cache_Get_Monitor`, `Cache_Set_MonitorReset` |

```c
cache_ModuleVersion_t Cache_Get_ModuleVersion  ( void );

cache_RequestState_t  Cache_Init               ( const cache_Config_t * const cacheConfig );
cache_RequestState_t  Cache_Deinit             ( void );
void                  Cache_Task               ( void );
cache_RequestState_t  Cache_Get_DefaultConfig  ( cache_Config_t * const cacheConfig );

cache_RequestState_t  Cache_Set_Active         ( cache_CacheId_t cacheId );
cache_RequestState_t  Cache_Set_Inactive       ( cache_CacheId_t cacheId );
cache_RequestState_t  Cache_Get_State          ( cache_CacheId_t cacheId, cache_State_t * const cacheState );

cache_RequestState_t  Cache_Set_Suspend        ( cache_CacheId_t cacheId );
cache_RequestState_t  Cache_Set_Resume         ( cache_CacheId_t cacheId );

cache_RequestState_t  Cache_Set_Invalidate     ( cache_CacheId_t cacheId );

cache_RequestState_t  Cache_Get_Monitor        ( cache_CacheId_t cacheId, cache_Monitor_t * const cacheMonitor );
cache_RequestState_t  Cache_Set_MonitorReset   ( cache_CacheId_t cacheId );
```

Every function returns `CACHE_REQUEST_OK` / `CACHE_REQUEST_ERROR` and checks its parameters and the state of the module
before it touches a register.

| Type | Meaning |
|---|---|
| `cache_CacheId_t` | `CACHE_INSTRUCTION`, `CACHE_DATA` |
| `cache_State_t` | `Requested` (flag), `Enabled` (flag, the real hardware state), `SuspendCnt` (counter) |
| `cache_Monitor_t` | `Hit` and `Miss` counters (where the hardware has them) |
| `cache_Config_t` | Requested state of every cache, associativity (where selectable), monitors on / off |

`Cache_Task()` is the common `Init` / `Deinit` / `Task` interface of the MCAL modules. The first version uses it for
nothing that the correctness depends on (an optional check that nobody changed the enable bit behind the back of the
module, and the end of a non-blocking invalidation if the family needs it); a module that is never served by it works
correctly.

---

## Usage

### Reading memory that must not be read through the cache

```c
(void)Cache_Set_Suspend( CACHE_INSTRUCTION );       /* the cache is off (if it was on) and nobody can switch it on */

flashSizeKb = *(volatile uint16_t *)FLASH_SIZE_REGISTER;

(void)Cache_Set_Resume( CACHE_INSTRUCTION );        /* the cache is on again (if it was on before and is requested) */
```

The bracket is short, it is used by the flash module for the OTP area, the unique ID and the high-cycle data flash of
STM32H5. Calls of other users nest: the cache is enabled again by the last `Cache_Set_Resume`.

### Cache and flash write

```c
/* after an erase or a write of the flash, before the changed memory is read through the cache */
(void)Cache_Set_Invalidate( CACHE_INSTRUCTION );
```

### Initialization

```c
cache_Config_t cacheConfig;

(void)Cache_Get_DefaultConfig( &cacheConfig );      /* the caches in the state in which they were found */
(void)Cache_Init( &cacheConfig );
```

Until the clock module hands the caches over, `Cache_Init` adopts the state that was found (the clock module may have
enabled the caches); `Cache_Set_Active` / `Cache_Set_Inactive` then manage it.

---

## Behaviour and limits

- **Short bypass.** With the cache suspended the code runs slower; keep the bracket short. The cache is cold after the
  resume (the content is lost while it is disabled or invalidated again when it is enabled - to be fixed by the
  family implementation).
- **Interrupts.** The suspend and the resume mask the interrupts for a few instructions. An interrupt that arrives
  inside the bracket runs without the cache.
- **Counter.** The suspend counter is 8 bit; an overflow returns an error and changes nothing. A missing resume leaves
  the cache off (slower code, no functional failure) - the counter is visible in `Cache_Get_State`.
- **MPU.** The module does not set the memory attributes. A region that has to be accessed by a pointer without the
  bracket needs a non-cacheable MPU region.
- **Not part of the first version:** the maintenance by address, the remap regions of the ICACHE, the interrupts and the
  error callbacks of the cache, TrustZone aliases of the registers.

---

## Repository layout and branches

The master holds this README and the license. The family branches (`Dev/<Family>`, `Releases/<Family>`) hold:

| Path | Content |
|---|---|
| `README.md` | README of the family: the caches of the device, specifics, status |
| `Cache_Port.h`, `Cache_Types.h` | Public interface (identical in all families) |
| `Cache*.c`, `Cache.h` | Implementation of the family |
| `CMakeLists.txt` | Library `Cache_Lib` of the EmBi platform |
| `Tests/UnitTests` | Unit tests (Unity + CMock + RegMem) |
| `Tests/IntegrationTests` | Integration tests on the target (probe-rs) |

A change of the public interface is made in all families.

---

## License

This project is licensed under the **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)**.

You are free to use, modify, and share this work for **non-commercial purposes**, provided appropriate credit is given.

See [LICENSE.md](LICENSE.md) for full terms or visit [creativecommons.org/licenses/by-nc/4.0](https://creativecommons.org/licenses/by-nc/4.0/).

---

## Authors

- **Mr.Nobody** - [embedbits.com](https://embedbits.com)

Contributions are welcome! Please open a pull request.
