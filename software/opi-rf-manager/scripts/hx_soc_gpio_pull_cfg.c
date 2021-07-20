#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// gcc -o hx_soc_gpio_pull_cfg hx_soc_gpio_pull_cfg.c
// Usage: hx_soc_gpio_pull_cfg <pin> <OFF|UP|DOWN>
// <pin> is an integer, the same you would write to /sys/class/gpio/export

int main(int argc, char *argv[])
{
    if(argc < 3)
    {
        fprintf(stderr, "Invalid argument count\n");

        return 1;
    }

    int pin = atoi(argv[1]);

    if(!(pin >= 32 * 0 + 0 && pin < 32 * 0 + 22) &&
       !(pin >= 32 * 2 + 0 && pin < 32 * 2 + 19) &&
       !(pin >= 32 * 3 + 0 && pin < 32 * 3 + 18) &&
       !(pin >= 32 * 4 + 0 && pin < 32 * 4 + 16) &&
       !(pin >= 32 * 5 + 0 && pin < 32 * 5 + 7) &&
       !(pin >= 32 * 6 + 0 && pin < 32 * 6 + 14) &&
       !(pin >= 32 * 11 + 0 && pin < 32 * 11 + 12))
    {
        fprintf(stderr, "Invalid pin number\n");

        return 1;
    }

    uint8_t mode = 0;

    if(!strcmp(argv[2], "OFF"))
    {
        mode = 0;
    }
    else if(!strcmp(argv[2], "UP"))
    {
        mode = 1;
    }
    else if(!strcmp(argv[2], "DOWN"))
    {
        mode = 2;
    }
    else
    {
        fprintf(stderr, "Invalid pull type\nSupported: OFF, UP, DOWN\n");

        return 1;
    }

    uint32_t page_size = sysconf(_SC_PAGESIZE);
	uint32_t addr_start = 0x01C20800 & ~(page_size - 1);
    uint32_t addr_offset = 0x01C20800 & (page_size - 1);

	int fd = open("/dev/mem", O_RDWR);

	if(fd < 0)
    {
        fprintf(stderr, "Unable to open /dev/mem\n");

        return 1;
    }

	void *pmapped = mmap(0, ((6 * 0x24 + 0x20) / page_size) + 1, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr_start);

	if(pmapped == MAP_FAILED)
    {
        fprintf(stderr, "Unable to map memory\n");

        return 1;
    }

    void *pbase = (void *)((uintptr_t)pmapped + addr_offset);

    uint8_t n = pin % 32;
    uint8_t p = pin / 32;

    uint32_t *pport_pull = (uint32_t *)((uintptr_t)pbase + (p * 0x24 + (n < 16 ? 0x1C : 0x20)));

    printf("port %hhu\n", p);
    printf("pin %hhu\n", n);
    printf("p%hhu_pull%hhu addr 0x%08X, old val 0x%08X\n", p, n >= 16, (uint32_t)pport_pull, *pport_pull);

    *pport_pull = (*pport_pull & ~(3 << ((n % 16) * 2))) | (mode << ((n % 16) * 2));

    printf("p%hhu_pull%hhu addr 0x%08X, new val 0x%08X\n", p, n >= 16, (uint32_t)pport_pull, *pport_pull);

    return 0;
}
