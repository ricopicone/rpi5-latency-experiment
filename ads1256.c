/* ============================================================================
 * ads1256.c  --  TI ADS1256 driver implementation
 * ==========================================================================*/

#include "ads1256.h"
#include "config.h"
#include "gpio.h"
#include "spidev_util.h"

#include <stdio.h>
#include <time.h>

/* ---- ADS1256 SPI command set ------------------------------------------- */
#define CMD_WAKEUP   0x00
#define CMD_RDATA    0x01
#define CMD_RDATAC   0x03
#define CMD_SDATAC   0x0F
#define CMD_RREG     0x10   /* OR with first register address               */
#define CMD_WREG     0x50   /* OR with first register address               */
#define CMD_SELFCAL  0xF0
#define CMD_SYNC     0xFC
#define CMD_RESET    0xFE

/* ---- Register addresses ------------------------------------------------ */
#define REG_STATUS   0x00
#define REG_MUX      0x01
#define REG_ADCON    0x02
#define REG_DRATE    0x03

/* Busy-poll cap for DRDY so a wiring fault cannot hang the program. At
 * 30 kSPS a conversion arrives every ~33 us; this is many conversions. */
#define DRDY_POLL_LIMIT  20000000L

static void delay_us(long us)
{
    struct timespec ts = { us / 1000000L, (us % 1000000L) * 1000L };
    nanosleep(&ts, NULL);
}

/* Send a single one-byte command inside its own CS frame. */
static void send_cmd(int fd, uint8_t cmd)
{
    gpio_adc_cs(1);
    spi_xfer(fd, &cmd, NULL, 1, ADS1256_SPI_HZ);
    gpio_adc_cs(0);
}

/* Read one register (used during init to verify communication). */
static uint8_t read_reg(int fd, uint8_t reg)
{
    uint8_t tx[2] = { (uint8_t)(CMD_RREG | reg), 0x00 /* count - 1 = 0 */ };
    uint8_t val   = 0;

    gpio_adc_cs(1);
    spi_xfer(fd, tx, NULL, 2, ADS1256_SPI_HZ);
    delay_us(10);                      /* t6: command -> data (>= 50 t_CLKIN) */
    spi_xfer(fd, NULL, &val, 1, ADS1256_SPI_HZ);
    gpio_adc_cs(0);
    return val;
}

int ads1256_wait_drdy(void)
{
    for (long i = 0; i < DRDY_POLL_LIMIT; i++) {
        int s = gpio_drdy_is_low();
        if (s < 0)  return -1;
        if (s == 1) return 0;
    }
    fprintf(stderr, "ads1256: timeout waiting for DRDY "
                    "(check wiring / GPIO line %d)\n", LINE_ADC_DRDY);
    return -1;
}

int ads1256_init(int fd)
{
    /* ---- hardware reset pulse ------------------------------------------ */
    gpio_adc_reset(0);   /* RESET high = run                                */
    delay_us(100);
    gpio_adc_reset(1);   /* RESET low  = reset                              */
    delay_us(100);
    gpio_adc_reset(0);   /* back to run                                     */
    delay_us(5000);      /* allow oscillator + power-up to settle           */

    /* Stop continuous-read mode in case it was left on. */
    send_cmd(fd, CMD_SDATAC);
    delay_us(100);

    /* ---- verify communication ----------------------------------------- */
    uint8_t status = read_reg(fd, REG_STATUS);
    uint8_t id     = (uint8_t)(status >> 4);   /* factory ID nibble = 0x3   */
    if (id != 0x3) {
        fprintf(stderr,
            "ads1256: unexpected STATUS=0x%02X (ID nibble 0x%X, expected 0x3).\n"
            "         Check SPI wiring, CS line, and SPI mode/clock.\n",
            status, id);
        /* Continue anyway -- caller may still want to attempt a run. */
    } else {
        printf("ads1256: detected (STATUS=0x%02X)\n", status);
    }

    /* ---- configure: STATUS, MUX, ADCON, DRATE in one WREG burst -------- */
    uint8_t wreg[6] = {
        (uint8_t)(CMD_WREG | REG_STATUS),   /* write starting at STATUS     */
        0x03,                               /* number of registers - 1 = 3 */
        ADS1256_STATUS,
        ADS1256_MUX,
        ADS1256_ADCON,
        ADS1256_DRATE
    };
    gpio_adc_cs(1);
    spi_xfer(fd, wreg, NULL, sizeof(wreg), ADS1256_SPI_HZ);
    gpio_adc_cs(0);
    delay_us(100);

    /* ---- self-calibration --------------------------------------------- */
    send_cmd(fd, CMD_SELFCAL);
    if (ads1256_wait_drdy() < 0) {           /* DRDY low when cal completes  */
        fprintf(stderr, "ads1256: self-calibration did not complete\n");
        return -1;
    }

    /* ---- enter continuous-read mode ----------------------------------- */
    send_cmd(fd, CMD_RDATAC);
    delay_us(10);    /* t6: RDATAC command -> first data clock               */

    printf("ads1256: configured (DRATE=0x%02X, MUX=0x%02X, RDATAC active)\n",
           ADS1256_DRATE, ADS1256_MUX);
    return 0;
}

int32_t ads1256_read_sample(int fd)
{
    uint8_t rx[3] = { 0, 0, 0 };

    gpio_adc_cs(1);
    spi_xfer(fd, NULL, rx, 3, ADS1256_SPI_HZ);
    gpio_adc_cs(0);

    int32_t v = ((int32_t)rx[0] << 16) |
                ((int32_t)rx[1] << 8)  |
                ((int32_t)rx[2]);
    if (v & 0x00800000)            /* sign-extend 24-bit two's complement   */
        v |= (int32_t)0xFF000000;
    return v;
}

double ads1256_to_volts(int32_t raw)
{
    /* PGA = 1: full scale code +0x7FFFFF corresponds to +V_REF. */
    return (double)raw / 8388607.0 * ADC_VREF;
}
