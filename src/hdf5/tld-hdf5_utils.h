#ifndef TLD_HDF5_UTILS_H
#define TLD_HDF5_UTILS_H

#include "stdint.h"

#include "hdf5.h"

#ifdef TLDHDF5_UTILS_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif


/*
  These functions determine the pointer to
   the block of memory to be written to hdf5.
 */
#define STARTOF_DATA(type)                                      \
        int startof_galloc_ ##type(type x, void** ptr);         \
        int startof_galloc_ ##type##_s(type* x, void** ptr);    \
        int startof_galloc_ ##type##_ss(type** x, void** ptr);

STARTOF_DATA(char)
STARTOF_DATA(int8_t)
STARTOF_DATA(uint8_t)
STARTOF_DATA(int16_t)
STARTOF_DATA(uint16_t)
STARTOF_DATA(int32_t)
STARTOF_DATA(uint32_t)
STARTOF_DATA(int64_t)
STARTOF_DATA(uint64_t)
STARTOF_DATA(float)
STARTOF_DATA(double)

int startof_galloc_unknown(void* x, void** ptr);

#undef STARTOF_DATA

#define SETTYPR(t)                                      \
        int set_type_ ## t(hid_t* type);

SETTYPR(char)
SETTYPR(int8_t)
SETTYPR(uint8_t)
SETTYPR(int16_t)
SETTYPR(uint16_t)
SETTYPR(int32_t)
SETTYPR(uint32_t)
SETTYPR(int64_t)
SETTYPR(uint64_t)
SETTYPR(float)
SETTYPR(double)

int set_type_unknown(hid_t *type);

#undef SETTYPR

#define HDFWRAP_SET_TYPE(P,T) _Generic((P),                             \
                                       char: set_type_char,             \
                                       char*: set_type_char,            \
                                       char**: set_type_char,           \
                                       char***: set_type_char,          \
                                       int8_t: set_type_int8_t,         \
                                       int8_t*: set_type_int8_t,        \
                                       int8_t**: set_type_int8_t,       \
                                       int8_t***: set_type_int8_t,      \
                                       uint8_t: set_type_uint8_t,       \
                                       uint8_t*: set_type_uint8_t,      \
                                       uint8_t**: set_type_uint8_t,     \
                                       uint8_t***: set_type_uint8_t,    \
                                       int16_t: set_type_int16_t,       \
                                       int16_t*: set_type_int16_t,      \
                                       int16_t**: set_type_int16_t,     \
                                       int16_t***: set_type_int16_t,    \
                                       uint16_t: set_type_uint16_t,     \
                                       uint16_t*: set_type_uint16_t,    \
                                       uint16_t**: set_type_uint16_t,   \
                                       uint16_t***: set_type_uint16_t,  \
                                       int32_t: set_type_int32_t,       \
                                       int32_t*: set_type_int32_t,      \
                                       int32_t**: set_type_int32_t,     \
                                       int32_t***: set_type_int32_t,    \
                                       uint32_t: set_type_uint32_t,     \
                                       uint32_t*: set_type_uint32_t,    \
                                       uint32_t**: set_type_uint32_t,   \
                                       uint32_t***: set_type_uint32_t,  \
                                       int64_t: set_type_int64_t,       \
                                       int64_t*: set_type_int64_t,      \
                                       int64_t**: set_type_int64_t,     \
                                       int64_t***: set_type_int64_t,    \
                                       uint64_t: set_type_uint64_t,     \
                                       uint64_t*: set_type_uint64_t,    \
                                       uint64_t**: set_type_uint64_t,   \
                                       uint64_t***: set_type_uint64_t,  \
                                       float: set_type_float,           \
                                       float*: set_type_float,          \
                                       float**: set_type_float,         \
                                       float***: set_type_float,        \
                                       double: set_type_double,         \
                                       double*: set_type_double,        \
                                       double**: set_type_double,       \
                                       double***: set_type_double,      \
                                       default: set_type_unknown        \
                )(T)




#undef TLDHDF5_UTILS_IMPORT
#undef EXTERN


#endif
