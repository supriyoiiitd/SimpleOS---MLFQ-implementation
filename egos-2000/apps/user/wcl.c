#include "app.h"
#include <string.h>
#include <stdio.h>

/* Static buffer + 1 byte for safety to ensure null-termination */
static char buf[BLOCK_SIZE + 1];

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("0\n");
        return 0;
    }

    int total_lines = 0;

    for (int i = 1; i < argc; i++) {
        int file_ino = dir_lookup(workdir_ino, argv[i]);
        if (file_ino < 0) {
            printf("wcl: cannot open %s\n", argv[i]);
            continue;
        }

        int offset = 0;
        while (1) {
            // 1. Clear buffer (including the +1 safety byte)
            memset(buf, 0, BLOCK_SIZE + 1);

            // 2. Read the block
            int status = file_read(file_ino, offset, buf);
            
            // If the driver returns error (like offset too large), stop.
            if (status < 0) break;

            // 3. Check data length
            int len = strlen(buf);
            
            // If empty, we are done
            if (len == 0) break;

            // 4. Count lines
            for (int k = 0; k < len; k++) {
                if (buf[k] == '\n') {
                    total_lines++;
                }
            }
            
            // 5. CRITICAL: If the block wasn't full, we reached EOF.
            // Break NOW to avoid the "TDERR: offset too large" error on next loop.
            if (len < BLOCK_SIZE) break;

            offset += BLOCK_SIZE;
        }
    }

    printf("%d\n", total_lines);
    return 0;
}