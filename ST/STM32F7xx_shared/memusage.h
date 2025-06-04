/******************************************************************************
 * INCLUDE FILES
 ******************************************************************************/

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define STACK_MAGIC         0xDEADBEEF

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

void stack_fill_with_magic(void); /* call at init to mark unused stack */
size_t stack_total(void); /* total stack size in bytes */
size_t stack_usage(void); /* est. worst case stack usage in bytes */

size_t heap_usage(void); /* implemented in sysmem.c */
size_t heap_total(void); /* total heap size in bytes */

// For Doom:
size_t Z_ZoneUsage(void);
unsigned int Z_ZoneSize(void);
