#ifndef HARDWARE_STRUCTS_ROSC_H
#define HARDWARE_STRUCTS_ROSC_H
struct rosc_hw_t {
    unsigned int randombit;
};
extern rosc_hw_t rosc_hw_obj;
#define rosc_hw (&rosc_hw_obj)
#endif
