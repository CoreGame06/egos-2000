#include "app.h"
#include "disk.h"

#define HRES 800
#define VRES 600

int main() {
    // 显示 tools/images/Bohr.bmp，图片已由 mkfs 放进 Flash 的第 5 个固定槽。
    // 按本例的 56 字节头、每像素 3 字节布局，定位到最后一个像素。
    char* rgb = (char*)(FLASH_ROM_BASE + EGOS_BIN_MAX_NBYTE * 5) +
                (56 + HRES * VRES * 3) - 3;

    // boot.c 已设置 800×600 显示；逐个读取图片像素，写入显存。
    for (uint i = 0; i < VRES; i++) {
        for (uint j = 0; j < HRES; j++) {
            // 每个显存像素占 4 字节；当前 HRES-j 写法在 j=0 时会落到下一行起点，存在一列偏移。
            uint* pix = (uint*)(VIDEO_FRAME_BASE + 4 * (i * HRES + (HRES - j)));

            // 填入图片中的 3 字节颜色，第 4 字节不用；再向前取一个像素。
            *pix = rgb[0] | (((uint)rgb[1]) << 8) | (((uint)rgb[2]) << 16);
            rgb -= 3;
        }
    }
}
