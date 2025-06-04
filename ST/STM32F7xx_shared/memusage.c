/******************************************************************************
 * INCLUDE FILES
 ******************************************************************************/

#include "stm32f7xx.h"
#include "memusage.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * GLOBAL DATA DEFINITIONS
 ******************************************************************************/

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/

/******************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 ******************************************************************************/

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

/******************************************************************************
 * FUNCTION BODIES
 ******************************************************************************/

size_t stack_total(void)
{
    extern uint32_t _stack_start;
    extern uint32_t _estack;

    uint32_t *start = &_stack_start;
    uint32_t *end = &_estack;

    size_t total = (uintptr_t)(end - start) * sizeof(uint32_t);
    return total;
}

void stack_fill_with_magic(void)
{
    extern uint32_t _stack_start;
    register uint32_t *sp __asm("sp");
    uint32_t *p = sp;
    while (p > &_stack_start)
    {
        *--p = 0xDEADBEEF;
    }
}

size_t stack_usage(void)
{
    extern uint32_t _stack_start;
    extern uint32_t _estack;

    uint32_t *start = &_stack_start;
    uint32_t *end = &_estack;

    if (start >= end)
        return 0;  // invalid

    // Binary search: find first non-MAGIC-value
    while (start < end)
    {
        uint32_t *mid = start + (end - start) / 2;

        if (*mid == STACK_MAGIC)
        {
            // Magic still ok -> search upward
            start = mid + 1;
        }
        else
        {
            // Magic destroyed → Stack has touched this mark
            end = mid;
        }
    }

    size_t used = ((uintptr_t)&_estack - (uintptr_t)start);
    return used;
}

