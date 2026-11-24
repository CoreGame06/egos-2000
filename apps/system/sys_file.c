/* 文件服务（PID 3，用户态）：接收文件块读写请求，调用文件系统并回复结果。
 * fs->read/write 按文件编号读写；下面的 read/write 按分区块号读写磁盘。 */

#include "app.h"
#include "inode.h"

// 分区容量以块数返回：2MiB / 512 = 4096 块。bs、ino 是统一接口参数，这一层不使用。
int getsize(inode_intf bs, uint ino) { return FILE_SYS_DISK_SIZE / BLOCK_SIZE; }

// 分区容量固定，不能调整大小。
int setsize(inode_intf bs, uint ino, uint newsize) { FATAL("cannot set size"); }

// 读一个 512 字节块：offset 是分区内块号，要加上分区起始块号才能读取整盘。
// 例如 offset=35，实际读取整盘第 4096+35 块。
int read(inode_intf bs, uint ino, uint offset, block_t* block) {
    earth->disk_read(FILE_SYS_DISK_START + offset, 1, block->bytes);
    return 0;
}

// 把 block 中的数据写入分区第 offset 块。
int write(inode_intf bs, uint ino, uint offset, block_t* block) {
    earth->disk_write(FILE_SYS_DISK_START + offset, 1, block->bytes);
    return 0;
}

int main() {
    SUCCESS("Enter kernel process GPID_FILE");

    // 登记分区读写接口，供文件系统调用。
    struct inode_store disk = (struct inode_store){
        .read = read, .write = write, .getsize = getsize, .setsize = setsize};

    // 当前 FILESYS=0，使用 file0.c 的 mydisk；挂接已有文件系统，不重新格式化。
    inode_intf fs =
        (FILESYS == 0) ? mydisk_init(&disk, 0) : treedisk_init(&disk, 0);

    // 文件请求 -> fs 查出对应的分区块 -> disk 接口 -> Earth 磁盘驱动。

    // 通知 sys_proc 文件服务已准备好，随后它会启动 Shell。
    char buf[SYSCALL_MSG_LEN];
    strcpy(buf, "Finish GPID_FILE initialization");
    grass->sys_send(GPID_PROCESS, buf, 32);

    // 循环接收文件请求，没有消息时阻塞。
    while (1) {
        int sender, r;
        // 请求和回复共用 buf；sender 是请求者 PID，r=0 表示操作成功。
        struct file_request* req = (void*)buf;
        struct file_reply* reply = (void*)buf;
        grass->sys_recv(GPID_ALL, &sender, buf, SYSCALL_MSG_LEN);

        switch (req->type) {
        case FILE_READ:
            // 读取文件 ino 的第 offset 块（512 字节），把内容和状态回复给请求者。
            r = fs->read(fs, req->ino, req->offset, (void*)&reply->block);
            reply->status = r == 0 ? FILE_OK : FILE_ERROR;
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            break;
        case FILE_WRITE:
            // 将请求自带的 block 写入指定文件块，回复成功/失败状态。
            r = fs->write(fs,req->ino,req->offset,(void*)&req->block);
            reply->status = r == 0? FILE_OK:FILE_ERROR;
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            break;
        default:
            FATAL("sys_file: invalid request %d", req->type);
        }
    }
}
