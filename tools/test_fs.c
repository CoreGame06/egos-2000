/*
 * Test Harness for mydisk file system (file0.c)
 * Automated test suite + Trace file (trace.txt) executor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MKFS 1
#include "inode.h"

// Include file0.c implementation directly
#include "file0.c"

#define DISK_BLOCKS 4096
static char ramdisk_buf[DISK_BLOCKS * BLOCK_SIZE];

static int ram_read(inode_intf self, uint ino, uint offset, block_t* block) {
    if (offset >= DISK_BLOCKS) return -1;
    memcpy(block, ramdisk_buf + offset * BLOCK_SIZE, BLOCK_SIZE);
    return 0;
}

static int ram_write(inode_intf self, uint ino, uint offset, block_t* block) {
    if (offset >= DISK_BLOCKS) return -1;
    memcpy(ramdisk_buf + offset * BLOCK_SIZE, block, BLOCK_SIZE);
    return 0;
}

static int ram_getsize(inode_intf self, uint ino) {
    return DISK_BLOCKS;
}

static int ram_setsize(inode_intf self, uint ino, uint newsize) {
    return DISK_BLOCKS;
}

static struct inode_store raw_disk = {
    .read = ram_read,
    .write = ram_write,
    .getsize = ram_getsize,
    .setsize = ram_setsize
};

static void print_result(const char* test_name, int success) {
    if (success) {
        printf("  [PASS] %s\n", test_name);
    } else {
        printf("  [FAIL] %s\n", test_name);
        exit(1);
    }
}

static void run_trace_file(inode_intf fs, const char* trace_filename) {
    FILE* fp = fopen(trace_filename, "r");
    if (!fp) {
        printf("  [SKIP] Trace file %s not found\n", trace_filename);
        return;
    }

    printf("\n--- Executing Trace File (%s) ---\n", trace_filename);
    char line[256];
    int line_num = 0;
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        // Remove trailing newline
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0 || line[0] == '#') continue;

        char op;
        uint ino, offset;
        char data[128];

        if (sscanf(line, "%c:%u:%u:%127s", &op, &ino, &offset, data) >= 3) {
            block_t b;
            memset(&b, 0, sizeof(block_t));

            if (op == 'W') {
                strncpy(b.bytes, data, BLOCK_SIZE);
                int res = fs->write(fs, ino, offset, &b);
                char msg[256];
                sprintf(msg, "Line %d: Trace WRITE W:%u:%u:%s", line_num, ino, offset, data);
                print_result(msg, res == 0);
            } else if (op == 'R') {
                int res = fs->read(fs, ino, offset, &b);
                char msg[256];
                sprintf(msg, "Line %d: Trace READ R:%u:%u:%s", line_num, ino, offset, data);
                print_result(msg, res == 0 && strncmp(b.bytes, data, strlen(data)) == 0);
            }
        } else if (sscanf(line, "%c:%u:%u", &op, &ino, &offset) == 3) {
            if (op == 'S') {
                fs->setsize(fs, ino, offset);
                char msg[256];
                sprintf(msg, "Line %d: Trace SETSIZE S:%u:%u", line_num, ino, offset);
                print_result(msg, 1);
            } else if (op == 'G') {
                int sz = fs->getsize(fs, ino);
                char msg[256];
                sprintf(msg, "Line %d: Trace GETSIZE G:%u (expected %u, got %d)", line_num, ino, offset, sz);
                print_result(msg, sz == (int)offset);
            }
        }
    }
    fclose(fp);
}

int main() {
    printf("=========================================\n");
    printf("   Running File System Automated Tests   \n");
    printf("=========================================\n\n");

    // 1. Format Disk
    printf("1. Testing Disk Formatting (mydisk_create)...\n");
    memset(ramdisk_buf, 0xAB, sizeof(ramdisk_buf)); // Fill disk with garbage first
    int create_res = mydisk_create(&raw_disk, 0, NINODES);
    print_result("mydisk_create returned 0", create_res == 0);

    inode_intf fs = mydisk_init(&raw_disk, 0);
    print_result("mydisk_init returned valid pointer", fs != NULL);

    // Verify Superblock
    block_t sb_block;
    raw_disk.read(&raw_disk, 0, 0, &sb_block);
    struct superblock* sb = (struct superblock*)sb_block.bytes;
    print_result("Superblock I == 2", sb->I == 2);
    print_result("Superblock F == 32", sb->F == 32);
    print_result("Superblock free_list_head == 35", sb->free_list_head == 35);

    // 2. Test Basic Write & Read
    printf("\n2. Testing Basic Read & Write...\n");
    block_t wblock, rblock;
    memset(&wblock, 0, sizeof(block_t));
    sprintf(wblock.bytes, "Hello, EGOS File System Test Data!");

    int write_res = fs->write(fs, 1, 0, &wblock); // Write to ino #1, offset 0
    print_result("fs->write ino #1 offset 0 returned 0", write_res == 0);

    int getsize_res = fs->getsize(fs, 1);
    print_result("fs->getsize ino #1 == 1 block", getsize_res == 1);

    memset(&rblock, 0, sizeof(block_t));
    int read_res = fs->read(fs, 1, 0, &rblock);
    print_result("fs->read ino #1 offset 0 returned 0", read_res == 0);
    print_result("Read data matches written data", strcmp(rblock.bytes, wblock.bytes) == 0);

    // 3. Test Auto Expansion
    printf("\n3. Testing Auto Expansion (Writing to offset 3)...\n");
    memset(&wblock, 0, sizeof(block_t));
    sprintf(wblock.bytes, "Data at block 3");

    write_res = fs->write(fs, 1, 3, &wblock);
    print_result("fs->write ino #1 offset 3 returned 0", write_res == 0);

    getsize_res = fs->getsize(fs, 1);
    print_result("fs->getsize ino #1 expanded to 4 blocks", getsize_res == 4);

    memset(&rblock, 0, sizeof(block_t));
    read_res = fs->read(fs, 1, 3, &rblock);
    print_result("Read offset 3 matches written data", strcmp(rblock.bytes, wblock.bytes) == 0);

    // Read out of bounds check
    read_res = fs->read(fs, 1, 4, &rblock);
    print_result("Read out of bounds (offset 4) correctly returned error (-1)", read_res == -1);

    // 4. Test setsize Expansion
    printf("\n4. Testing setsize Expansion...\n");
    int old_sz = fs->setsize(fs, 2, 10);
    print_result("fs->setsize ino #2 to 10 returned old_size 0", old_sz == 0);
    getsize_res = fs->getsize(fs, 2);
    print_result("fs->getsize ino #2 == 10 blocks", getsize_res == 10);

    // 5. Test setsize Shrink
    printf("\n5. Testing setsize Shrink...\n");
    old_sz = fs->setsize(fs, 2, 3);
    print_result("fs->setsize ino #2 shrink to 3 returned old_size 10", old_sz == 10);
    getsize_res = fs->getsize(fs, 2);
    print_result("fs->getsize ino #2 == 3 blocks", getsize_res == 3);

    // 6. Test Freeing to 0 (nblocks == 0)
    printf("\n6. Testing File Truncation to 0 (nblocks == 0)...\n");
    old_sz = fs->setsize(fs, 1, 0);
    print_result("fs->setsize ino #1 to 0 returned old_size 4", old_sz == 4);
    getsize_res = fs->getsize(fs, 1);
    print_result("fs->getsize ino #1 == 0 blocks", getsize_res == 0);

    // 7. Run Trace File Test (trace.txt)
    run_trace_file(fs, "trace.txt");

    printf("\n=========================================\n");
    printf("  🎉 ALL FS AUTOMATED TESTS PASSED! 🎉  \n");
    printf("=========================================\n");

    return 0;
}
