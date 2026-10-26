#ifndef LUNA_HANDOFF_H
#define LUNA_HANDOFF_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* LUNA's linker reserves this scratch range until the loader installs EECORE.
 * ExecPS2/packed ELF startup accepts only 16 arguments and 256 string bytes.
 * Longer argument lists arrive as a reference to the resident 32-bit argv. */
#define LUNA_HANDOFF_MIN 0x000f4000U
#define LUNA_HANDOFF_END 0x000fe000U
#define LUNA_HANDOFF_MAX (12U * 1024U)

static inline int luna_handoff_expand(int *argc, char ***argv) {
    const char *prefix = "-luna-args=";
    if (*argc < 2 || strncmp((*argv)[1], prefix, strlen(prefix))) return 0;
    if (*argc != 2) return -1;
    const char *reference = (*argv)[1] + strlen(prefix);
    uint32_t fields[3] = {0};
    if (strlen(reference) != 26) return -1;
    for (int field = 0; field < 3; field++) {
        for (int digit = 0; digit < 8; digit++) {
            char c = *reference++;
            unsigned value;
            if (c >= '0' && c <= '9') value = c - '0';
            else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
            else return -1;
            fields[field] = (fields[field] << 4) | value;
        }
        if (field < 2 && *reference++ != ':') return -1;
    }
    uint32_t address = fields[0], bytes = fields[1], count = fields[2];
    if ((address & 3) || address < LUNA_HANDOFF_MIN || address >= LUNA_HANDOFF_END ||
        bytes > LUNA_HANDOFF_MAX || bytes > LUNA_HANDOFF_END - address ||
        !count || count > bytes / 5) return -1;

    char **copy = malloc((count + 1) * sizeof(*copy) + bytes);
    if (!copy) return -1;
    char *data = (char *)(copy + count + 1);
    memcpy(data, (const void *)(uintptr_t)address, bytes);
    uint32_t offset = count * sizeof(uint32_t);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pointer;
        memcpy(&pointer, data + i * sizeof(uint32_t), sizeof(pointer));
        if (offset >= bytes || pointer != address + offset) goto invalid;
        char *end = memchr(data + offset, 0, bytes - offset);
        if (!end) goto invalid;
        copy[i] = data + offset;
        offset = (uint32_t)(end - data) + 1;
    }
    if (offset != bytes) goto invalid;
    copy[count] = NULL;
    *argc = (int)count;
    *argv = copy;
    return 1;
invalid:
    free(copy);
    return -1;
}

#endif
