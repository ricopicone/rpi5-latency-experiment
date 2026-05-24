/* ============================================================================
 * spi_rp1.h -- Synopsys DesignWare APB SSI v2.04A register layout, used by
 *              the RP1 SPI0 peripheral on the Raspberry Pi 5.
 *
 * Reference: linux drivers/spi/spi-dw.h (raspberrypi/linux, rpi-6.12.y),
 * specifically the DW_PSSI_* bit-field macros for the DWC APB SSI variant.
 * RP1's VERSION register reads 0x3430322a = "204a".
 *
 * Phase 3 of the §6 feasibility plan -- header-only helpers for direct
 * register-access SPI transfers, bypassing `spidev`.
 * ==========================================================================*/
#ifndef FEASIBILITY_SPI_RP1_H
#define FEASIBILITY_SPI_RP1_H

#include <stdint.h>
#include <stddef.h>

/* RP1 SPI0 physical address and size (from /proc/iomem). */
#define RP1_SPI0_PHYS   0x1f00050000UL
#define RP1_SPI0_SIZE   0x00000200UL

/* RP1 SPI0 input clock (clk_sys = 200 MHz, confirmed via clk_summary). */
#define RP1_SPI0_CLK_HZ 200000000u

/* Register offsets (Synopsys DWC APB SSI, used by RP1). */
#define DW_CTRLR0       0x00
#define DW_CTRLR1       0x04
#define DW_SSIENR       0x08
#define DW_MWCR         0x0c
#define DW_SER          0x10
#define DW_BAUDR        0x14
#define DW_TXFTLR       0x18
#define DW_RXFTLR       0x1c
#define DW_TXFLR        0x20
#define DW_RXFLR        0x24
#define DW_SR           0x28
#define DW_IMR          0x2c
#define DW_DR           0x60
#define DW_VERSION      0x5c

/* CTRLR0 bit fields (PSSI variant). */
#define DW_CTRLR0_DFS32_SHIFT  16       /* bits [20:16], DFS-1 (e.g. 7 = 8-bit) */
#define DW_CTRLR0_DFS32_8BIT   (7u << DW_CTRLR0_DFS32_SHIFT)
#define DW_CTRLR0_FRF_MOTO_SPI 0u       /* bits [5:4]                          */
#define DW_CTRLR0_SCPHA        (1u << 6)
#define DW_CTRLR0_SCPOL        (1u << 7)
#define DW_CTRLR0_TMOD_TR      (0u << 8)
#define DW_CTRLR0_TMOD_TO      (1u << 8)
#define DW_CTRLR0_TMOD_RO      (2u << 8)

/* SR (status register) bits. */
#define DW_SR_BUSY             (1u << 0)
#define DW_SR_TFNF             (1u << 1)   /* TX FIFO not full      */
#define DW_SR_TFE              (1u << 2)   /* TX FIFO empty         */
#define DW_SR_RFNE             (1u << 3)   /* RX FIFO not empty     */
#define DW_SR_RFF              (1u << 4)   /* RX FIFO full          */

/* Convenience macro for a memory-mapped register access. */
#define R(base, off)  (*(volatile uint32_t *)((char *)(base) + (off)))

/* Pre-built CTRLR0 values for our two use cases (8-bit frames, SPI mode 1,
 * master mode is implicit in PSSI). */
#define DW_CTRLR0_RX_8BIT_MODE1 \
    (DW_CTRLR0_DFS32_8BIT | DW_CTRLR0_SCPHA | DW_CTRLR0_TMOD_RO)
#define DW_CTRLR0_TX_8BIT_MODE1 \
    (DW_CTRLR0_DFS32_8BIT | DW_CTRLR0_SCPHA | DW_CTRLR0_TMOD_TO)
#define DW_CTRLR0_TR_8BIT_MODE1 \
    (DW_CTRLR0_DFS32_8BIT | DW_CTRLR0_SCPHA | DW_CTRLR0_TMOD_TR)

#endif /* FEASIBILITY_SPI_RP1_H */
