#ifndef PICO_UNIQUE_ID_H
#define PICO_UNIQUE_ID_H
#define PICO_UNIQUE_ID_SIZE_BYTES 8
struct pico_unique_id_t {
    unsigned char id[PICO_UNIQUE_ID_SIZE_BYTES];
};
inline void pico_get_unique_id(pico_unique_id_t* id) {
    if (id) {
        for (int i = 0; i < PICO_UNIQUE_ID_SIZE_BYTES; i++) id->id[i] = i;
    }
}
#endif
