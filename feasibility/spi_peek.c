/* Quick diagnostic: mmap the RP1 SPI0 peripheral, print its registers.
 * Run this AFTER spidev has done a transfer at a known speed to back out
 * what BAUDR value spidev wrote, and confirm CTRLR0 layout. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define SPI0_PHYS  0x1f00050000UL
#define SPI0_SIZE  0x00000200UL

#define R(off) (*(volatile uint32_t *)((char *)map + (off)))

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }
    void *map = mmap(NULL, SPI0_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)SPI0_PHYS);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }

    printf("RP1 SPI0 registers (physical 0x%lx):\n", SPI0_PHYS);
    printf("  CTRLR0   0x00 = 0x%08x\n", R(0x00));
    printf("  CTRLR1   0x04 = 0x%08x\n", R(0x04));
    printf("  SSIENR   0x08 = 0x%08x\n", R(0x08));
    printf("  MWCR     0x0c = 0x%08x\n", R(0x0c));
    printf("  SER      0x10 = 0x%08x\n", R(0x10));
    printf("  BAUDR    0x14 = 0x%08x  (SCK = 200 MHz / BAUDR = %.3f MHz)\n",
           R(0x14), R(0x14) ? 200.0 / R(0x14) : 0);
    printf("  TXFTLR   0x18 = 0x%08x\n", R(0x18));
    printf("  RXFTLR   0x1c = 0x%08x\n", R(0x1c));
    printf("  TXFLR    0x20 = 0x%08x\n", R(0x20));
    printf("  RXFLR    0x24 = 0x%08x\n", R(0x24));
    printf("  SR       0x28 = 0x%08x\n", R(0x28));
    printf("  VERSION  0x5c = 0x%08x\n", R(0x5c));

    munmap(map, SPI0_SIZE);
    close(fd);
    return 0;
}
