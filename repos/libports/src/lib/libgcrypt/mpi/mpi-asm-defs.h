/*
 * 'libgcrypt/src/mpi.h' unconditionally includes "../mpi/mpi-asm-defs.h"/
 * We need to direct it to one of the architecture-specific versions at
 * 'libgcrypt/mpi/<architecture>/'.
 */

#include <../mpi/generic/mpi-asm-defs.h>
