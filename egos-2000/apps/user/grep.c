#include "app.h"
#include <string.h>
#include <stdio.h>

/* Static buffers to prevent stack overflow */
static char buf[BLOCK_SIZE + 1];
static char line_buf[2048];

int main(int argc, char *argv[]) {
    // Check arguments: grep [PATTERN] [FILE]
    if (argc != 3) {
        printf("Usage: grep [PATTERN] [FILE]\n");
        return 0;
    }

    char *pattern = argv[1];
    int file_ino = dir_lookup(workdir_ino, argv[2]);
    
    if (file_ino < 0) {
        printf("grep: cannot open %s\n", argv[2]);
        return 0;
    }

    int offset = 0;
    int line_idx = 0;

    while (1) {
        // 1. Clear buffer
        memset(buf, 0, BLOCK_SIZE + 1);

        // 2. Read block
        int status = file_read(file_ino, offset, buf);
        if (status < 0) break;

        // 3. Get length
        int len = strlen(buf);
        if (len == 0) break;

        // 4. Process characters
        for (int i = 0; i < len; i++) {
            char c = buf[i];
            
            if (c == '\n') {
                // End of line found
                line_buf[line_idx] = '\0'; 
                
                // Check for pattern
                if (strstr(line_buf, pattern) != NULL) {
                    printf("%s\n", line_buf);
                }
                
                // Reset line buffer
                line_idx = 0;
            } else {
                // Add char to line buffer
                if (line_idx < 2047) {
                    line_buf[line_idx++] = c;
                }
            }
        }
        
        // 5. Break if EOF (partial block)
        if (len < BLOCK_SIZE) break;
        
        offset += BLOCK_SIZE;
    }
    
    // Handle the very last line if the file doesn't end with \n
    if (line_idx > 0) {
        line_buf[line_idx] = '\0';
        if (strstr(line_buf, pattern) != NULL) {
            printf("%s\n", line_buf);
        }
    }

    return 0;
}