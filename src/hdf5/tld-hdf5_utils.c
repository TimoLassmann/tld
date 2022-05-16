#include "../core/tld-core.h"
#include "hdf5.h"
#define TLDHDF5_UTILS_IMPORT
#include "tld-hdf5_utils.h"

#define STARTOF_DATA(type)                                      \
        int startof_galloc_ ##type(type x, void** ptr)          \
        {                                                       \
                *ptr = &x;                                      \
                return OK;                                      \
        }                                                       \
        int startof_galloc_ ##type ## _s(type* x, void** ptr)   \
        {                                                       \
                *ptr = &x[0];                                   \
                return OK;                                      \
        }                                                       \
        int startof_galloc_ ##type ## _ss(type** x, void** ptr) \
        {                                                       \
                *ptr = &x[0][0];                                \
                return OK;                                      \
        }

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
#undef STARTOF_DATA


int startof_galloc_unknown(void* x, void** ptr)
{
        ERROR_MSG("unknown data type: %p %p", x,*ptr);
        return OK;
ERROR:
        return FAIL;
}


int set_type_char(hid_t* type)
{
        *type = H5T_NATIVE_CHAR;
        return OK;
}

int set_type_int8_t(hid_t* type)
{
        *type = H5T_NATIVE_INT8;
        return OK;
}

int set_type_uint8_t(hid_t* type)
{
        *type = H5T_NATIVE_UINT8;
        return OK;
}

int set_type_int16_t(hid_t* type)
{
        *type = H5T_NATIVE_INT16;
        return OK;
}

int set_type_uint16_t(hid_t* type)
{
        *type = H5T_NATIVE_UINT16;
        return OK;
}

int set_type_int32_t(hid_t* type)
{
        *type = H5T_NATIVE_INT32;
        return OK;
}

int set_type_uint32_t(hid_t* type)
{
        *type = H5T_NATIVE_UINT32;
        return OK;
}
int set_type_int64_t(hid_t* type)
{
        *type = H5T_NATIVE_INT64;
        return OK;
}

int set_type_uint64_t(hid_t* type)
{
        *type = H5T_NATIVE_UINT64;
        return OK;
}

int set_type_float(hid_t* type)
{
        *type = H5T_NATIVE_FLOAT;
        return OK;
}

int set_type_double(hid_t* type)
{
        *type = H5T_NATIVE_DOUBLE;
        return OK;
}


int set_type_unknown(hid_t* type)
{

        WARNING_MSG("Could not determine type! (%d)",type);
        return FAIL;
}
