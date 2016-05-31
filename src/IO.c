#include "IO.h"

#include <stdlib.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define PI_PERI_BASE             0x20000000
#define GPIO_BASE                (PI_PERI_BASE + 0x200000) /* GPIO controller */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

static volatile unsigned *gpio = 0;

/*
 *  GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
 */
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

/* sets bits which are 1 ignores bits which are 0 */
#define GPIO_SET *(gpio+7)
/* clears bits which are 1 ignores bits which are 0 */
#define GPIO_CLR *(gpio+10)

int rpiIO_initGPIO()
{
	int  mem_fd;
	void *gpio_map;

	/* open /dev/mem */
	if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
	  fprintf(stderr, "can't open /dev/mem \n");
	  return -1;
	}

	/* mmap GPIO */
	gpio_map = mmap(
	  NULL,             /* Any adddress in our space will do */
	  BLOCK_SIZE,       /* Map length */
	  PROT_READ | PROT_WRITE, /* Enable reading & writting to mapped memory */
	  MAP_SHARED,       /* Shared with other processes */
	  mem_fd,           /* File to map */
	  GPIO_BASE         /* Offset to GPIO peripheral */
	);

	close(mem_fd);

	if (gpio_map == MAP_FAILED) {
	  printf("mmap error %p\n", gpio_map);
	  return -1;
	}

	/* Always use volatile pointer!  */
	gpio = (volatile unsigned *)gpio_map;
}

void rpiIO_pinMode(int gpioPin, PINMODE mode)
{
	INP_GPIO(gpioPin); /* must use INP_GPIO before we can use OUT_GPIO  */

	if (mode == PINMODE_OUTPUT) {
		OUT_GPIO(gpioPin);
	}
}

void rpiIO_digitalWrite(int gpioPin, int value)
{
	if (value == 0) {
		GPIO_CLR = 1<<gpioPin;
	}
	else {
		GPIO_SET = 1<<gpioPin;
	}
}

typedef struct _RPIIO_SPI {
	uint8_t mode;
	uint8_t BPW;
	uint16_t delay;

	uint32_t maxClock;
	int fd;
} RPIIO_SPI;

RPIIO_SPI * rpiIO_spiCreate(const char * devName, int maxClock)
{
	int fd;

	if ((fd = open (devName, O_RDWR)) < 0) {
		return NULL;
	}

	RPIIO_SPI * p = malloc(sizeof(RPIIO_SPI));


	p->maxClock = maxClock;
	p->fd = fd;
	p->mode = 0;
	p->BPW = 8;
	p->delay = 0;

	if (ioctl (fd, SPI_IOC_WR_MODE, &p->mode)         < 0) return NULL;
	if (ioctl (fd, SPI_IOC_RD_MODE, &p->mode)         < 0) return NULL;

	if (ioctl (fd, SPI_IOC_WR_BITS_PER_WORD, &p->BPW) < 0) return NULL;
	if (ioctl (fd, SPI_IOC_RD_BITS_PER_WORD, &p->BPW) < 0) return NULL;

	if (ioctl (fd, SPI_IOC_WR_MAX_SPEED_HZ, &p->maxClock)   < 0) return NULL;
	if (ioctl (fd, SPI_IOC_RD_MAX_SPEED_HZ, &p->maxClock)   < 0) return NULL;

	return p;
}

void rpiIO_spiDestroy(RPIIO_SPI * p)
{
	if (!p) return;

	free(p);
}


int rpiIO_spiDataRW (RPIIO_SPI * p,  unsigned char * tx, unsigned char * rx, int len)
{
	if (!p) return;

	struct spi_ioc_transfer spi;

	spi.tx_buf        = (unsigned long)tx;
	spi.rx_buf        = (unsigned long)rx;
	spi.len           = len ;
	spi.delay_usecs   = p->delay;
	spi.speed_hz      = p->maxClock;
	spi.bits_per_word = p->BPW ;

	return ioctl (p->fd, SPI_IOC_MESSAGE(1), &spi);
}
