/*
 * Solaris DLPI driver for ethernet cards based on the Macronix 98715
 *
 * Copyright (c) 2001-2007 by Garrett D'Amore <garrett@damore.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MXFE_H
#define	_MXFE_H

#ident	"@(#)$Id: mxfe.h,v 1.4 2007/03/29 03:46:13 gdamore Exp $"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * These are conveniently defined to have the same values
 * as are used by the NDD utility, which is an undocumented
 * interface.  YMMV.
 */
#define	NDIOC	('N' << 8)
#define	NDIOC_GET	(NDIOC|0)
#define	NDIOC_SET	(NDIOC|1)

/*
 * Registers and values are here, becuase they can be exported to userland
 * via the MXFEIOC_GETCSR and friends ioctls.  These are private to this
 * driver and the bundled diagnostic utility, and should not be used by
 * end user application programs.
 */

/*
 * MXFE register definitions.
 */
/* PCI configuration registers */
#define	MXFE_PCI_VID	0x00	/* Loaded vendor ID */
#define	MXFE_PCI_DID	0x02	/* Loaded device ID */
#define	MXFE_PCI_COMM	0x04	/* Configuration command register */
#define	MXFE_PCI_STAT	0x06	/* Configuration status register */
#define	MXFE_PCI_RID	0x08	/* Revision ID */
#define	MXFE_PCI_PCC	0x09	/* Programming class code, not used */
#define	MXFE_PCI_SCC	0x0a	/* Subclass code, 00 == ethernet */
#define	MXFE_PCI_BCC	0x0b	/* Base class code, 02 == network */
#define	MXFE_PCI_CLS	0x0c	/* Cache line size */
#define	MXFE_PCI_LAT	0x0d	/* Latency timer */
#define	MXFE_PCI_IOBA	0x10	/* IO Base Address */
#define	MXFE_PCI_MBA	0x14	/* Memory Base Address */
#define	MXFE_PCI_CIS	0x28	/* Card Information Structure (CardBus) */
#define	MXFE_PCI_SVID	0x2c	/* Subsystem vendor ID */
#define	MXFE_PCI_SSID	0x2e	/* Subsystem ID */
#define	MXFE_PCI_BRBA	0x30	/* Boot ROM Base Address (ROM size = 256KB) */
#define	MXFE_PCI_CP	0x34	/* Capability Pointer */
#define	MXFE_PCI_ILINE	0x3c	/* Interrupt Line */
#define	MXFE_PCI_IPIN	0x3d	/* Interrupt Pin */
#define	MXFE_PCI_MINGNT	0x3e	/* Minimum Grant */
#define	MXFE_PCI_MAXLAT	0x3f	/* Maximum latency */
#define	MXFE_PCI_DS	0x40	/* Driver space for special purpose */
#define	MXFE_PCI_PMR0	0xc0	/* Power Management Register 0 */
#define	MXFE_PCI_PMR1	0xc4	/* Power Management Register 1 */

/*
 * Bits for PCI command register.
 */
#define	MXFE_PCI_BME	0x0004	/* bus master enable */
#define	MXFE_PCI_MAE	0x0002	/* memory access enable */
#define	MXFE_PCI_IOE	0x0001	/* I/O access enable */

/* Ordinary control/status registers */
#define	MXFE_CSR_PAR	0x00	/* PCI access register */
#define	MXFE_CSR_TDR	0x08	/* Transmit demand register */
#define	MXFE_CSR_RDR	0x10	/* Receive demand register */
#define	MXFE_CSR_RDB	0x18	/* Receive descriptor base address */
#define	MXFE_CSR_TDB	0x20	/* Transmit descriptor base address */
#define	MXFE_CSR_SR	0x28	/* Status register */
#define	MXFE_CSR_NAR	0x30	/* Network access register */
#define	MXFE_CSR_IER	0x38	/* Interrupt enable register */
#define	MXFE_CSR_LPC	0x40	/* Lost packet counter */
#define	MXFE_CSR_SPR	0x48	/* Serial port register */
#define	MXFE_CSR_10	0x50	/* reserved */
#define	MXFE_CSR_TIMER	0x58	/* Timer */
#define	MXFE_CSR_TSTAT	0x60	/* 10Base-T status */
#define	MXFE_CSR_SIA	0x68	/* SIA reset register */
#define	MXFE_CSR_TCTL	0x70	/* 10Base-T control */
#define	MXFE_CSR_WTMR	0x78	/* Watchdog timer */
#define	MXFE_CSR_MXMAGIC	0x80	/* MXIC magic register */
#define	MXFE_CSR_CR	0x88	/* Command register */
#define	MXFE_CSR_PCIC	0x8c	/* PCI bus performance counter */
#define	MXFE_CSR_PMCSR	0x90	/* Power Management Command and Status */
#define	MXFE_CSR_TXBR	0x9c	/* Transmit burst counter/time-out register */
#define	MXFE_CSR_FROM	0xa0	/* Flash(boot) ROM port */
#define	MXFE_CSR_ACOMP	0xa0	/* Autocompensation */

/*
 * Bits for PCI access register.
 */
#define	MXFE_RESET	0x00000001U	/* Reset the entire chip */
#define	MXFE_MWIE	0x01000000U	/* PCI memory-write-invalidate */
#define	MXFE_MRLE	0x00800000U	/* PCI memory-read-line */
#define	MXFE_MRME	0x00200000U	/* PCI memory-read-multiple */
#define	MXFE_TXHIPRI	0x00000002U	/* Transmit higher priority */
#define	MXFE_DESCSKIP	0x0000007cU	/* Descriptor skip length in DW */
#define	MXFE_BIGENDIAN	0x00000080U	/* Use big endian data buffers */
#define	MXFE_TXAUTOPOLL	0x00060000U	/* Programmable TX autopoll interval */
#define	MXFE_CALIGN_NONE	0x00000000U	/* No cache alignment */
#define	MXFE_CALIGN_8	0x00004000U	/* 8 DW cache alignment */
#define	MXFE_CALIGN_16	0x00008000U	/* 16 DW cache alignment */
#define MXFE_CALIGN_32	0x0000c000U	/* 32 DW cache alignment */
#define	MXFE_BURSTLEN	0x00003F00U	/* Programmable burst length */
#define	MXFE_BURSTUNL	0x00000000U	/* Unlimited burst length */
#define	MXFE_BURST_1	0x00000100U	/* 1 DW burst length */
#define	MXFE_BURST_2	0x00000200U	/* 2 DW burst length */
#define	MXFE_BURST_4	0x00000400U	/* 4 DW burst length */
#define	MXFE_BURST_8	0x00000800U	/* 8 DW burst length */
#define	MXFE_BURST_16	0x00001000U	/* 16 DW burst length */
#define	MXFE_BURST_32	0x00002000U	/* 32 DW burst length */

/*
 * Bits for status register.  Interrupt bits are also used by
 * the interrupt enable register.
 */
#define	MXFE_BERR_TYPE		0x03800000U	/* bus error type */
#define	MXFE_BERR_PARITY	0x00000000U	/* parity error */
#define	MXFE_BERR_TARGET_ABORT	0x01000000U	/* target abort */
#define	MXFE_BERR_MASTER_ABORT	0x00800000U	/* master abort */
#define	MXFE_INT_100LINK	0x08000000U	/* 100 Base-T link */
#define	MXFE_INT_NORMAL		0x00010000U	/* normal interrupt */
#define	MXFE_INT_ABNORMAL	0x00008000U	/* abnormal interrupt */
#define	MXFE_INT_EARLYRX	0x00004000U	/* early receive interrupt */
#define	MXFE_INT_BUSERR		0x00002000U	/* fatal bus error interrupt */
#define	MXFE_INT_10LINK		0x00001000U	/* 10 Base-T link */
#define	MXFE_INT_TIMER		0x00000800U	/* onboard timer interrupt */
#define	MXFE_INT_EARLYTX	0x00000400U	/* early transmit interrupt */
#define	MXFE_INT_RXJABBER	0x00000200U	/* receive watchdog timeout */
#define	MXFE_INT_RXIDLE		0x00000100U	/* receive idle interrupt */
#define	MXFE_INT_RXNOBUF	0x00000080U	/* no rcv descriptor */
#define	MXFE_INT_RXOK		0x00000040U	/* rcv complete interrupt */
#define	MXFE_INT_TXUNDERFLOW	0x00000020U	/* transmit underflow */
#define	MXFE_INT_ANEG		0x00000010U	/* autonegotiation */
#define	MXFE_INT_TXJABBER	0x00000008U	/* transmit jabber timeout */
#define	MXFE_INT_TXNOBUF	0x00000004U	/* no xmt descriptor */
#define	MXFE_INT_TXIDLE		0x00000002U	/* transmit idle interrupt */
#define	MXFE_INT_TXOK		0x00000001U	/* transmit ok interrupt */

#define	MXFE_INT_ALL		0x0801bbffU	/* all above interrupts */
#define	MXFE_INT_NONE		0x00000000U	/* no interrupts */
#define	MXFE_INT_WANTED		(MXFE_INT_NORMAL | MXFE_INT_ABNORMAL | \
				MXFE_INT_BUSERR | MXFE_INT_RXJABBER | \
				MXFE_INT_RXOK | MXFE_INT_TXUNDERFLOW | \
				MXFE_INT_RXIDLE | \
				MXFE_INT_RXNOBUF | MXFE_INT_TXJABBER | \
				MXFE_INT_100LINK | MXFE_INT_10LINK | \
				MXFE_INT_ANEG | MXFE_INT_TIMER)

/*
 * Bits for network access register.
 */
#define	MXFE_TX_ENABLE	0x00002000U	/* Enable transmit */
#define	MXFE_RX_MULTI	0x00000080U	/* Receive all multicast packets */
#define	MXFE_RX_PROMISC	0x00000040U	/* Receive any good packet */
#define	MXFE_RX_BAD	0x00000008U	/* Pass bad packets */
#define	MXFE_RX_ENABLE	0x00000002U	/* Enable receive */
#define MXFE_NAR_TR	0x0000c000U	/* Transmit threshold mask */
#define MXFE_NAR_TR_72	0x00000000U	/* 72 B (128 @ 100Mbps) tx thresh */
#define MXFE_NAR_TR_96	0x00004000U	/* 96 B (256 @ 100Mbps) tx thresh */
#define MXFE_NAR_TR_128	0x00008000U	/* 128 B (512 @ 100Mbps) tx thresh */
#define MXFE_NAR_TR_160	0x0000c000U	/* 160 B (1K @ 100Mbsp) tx thresh */
#define MXFE_NAR_SF	0x00200000U	/* store and forward */
#define	MXFE_NAR_SCR	0x01000000U	/* scrambler mode */
#define	MXFE_NAR_PCS	0x00800000U	/* set for forced 100 mbit */
#define	MXFE_NAR_SPEED	0x00400000U	/* transmit threshold, set for 10bt */
#define	MXFE_NAR_COE	0x00200000U	/* collision offset enable */
#define	MXFE_NAR_SF	0x00200000U	/* store and forward */
#define	MXFE_NAR_HBD	0x00080000U	/* Disable SQE heartbeat */
#define	MXFE_NAR_PORTSEL	0x00040000U	/* 1 = 100 mbit */
#define	MXFE_NAR_FDX	0x00000200U	/* 1 = full duplex */

/*
 * Bits for lost packet counter.
 */
#define	MXFE_LPC_COUNT	0x00007FFFU	/* Count of missed frames */
#define	MXFE_LPC_OFLOW	0x00008000U	/* Counter overflow bit */

/*
 * Bits for MXFE_CSR_SPR (MII and SROM access)
 */
#define	MXFE_MII_DIN	0x00080000U	/* MII data input */
#define	MXFE_MII_CONTROL	0x00040000U	/* MII management control, 1=read */
#define	MXFE_MII_DOUT	0x00020000U	/* MII data output */
#define	MXFE_MII_CLOCK	0x00010000U	/* MII data clock */
#define	MXFE_SROM_READ	0x00004000U	/* Serial EEPROM read control */
#define	MXFE_SROM_WRITE	0x00002000U	/* Serial EEPROM write control */
#define	MXFE_SROM_SEL	0x00000800U	/* Serial EEPROM select */
#define	MXFE_SROM_DOUT	0x00000008U	/* Serial EEPROM data out */
#define	MXFE_SROM_DIN	0x00000004U	/* Serial EEPROM data in */
#define	MXFE_SROM_CLOCK	0x00000002U	/* Serial EEPROM clock */
#define	MXFE_SROM_CHIP	0x00000001U	/* Serial EEPROM chip select */
#define	MXFE_SROM_ENADDR	0x70	/* Ethernet address pointer! */
#define	MXFE_SROM_READCMD	0x6	/* command to read SROM */

/*
 * Bits for MXFE_CSR_TIMER
 */
#define	MXFE_TIMER_LOOP	0x00010000U	/* continuous operating mode */
#define	MXFE_TIMER_USEC	204		/* usecs per timer count */

/*
 * Bits for TSTAT
 */
#define	MXFE_TSTAT_LPC	0xFFFF0000U	/* link partner's code word */
#define	MXFE_TSTAT_LPN	0x00008000U	/* link partner supports nway */
#define	MXFE_TSTAT_ANS	0x00007000U	/* autonegotiation state mask */
#define	MXFE_TSTAT_TRF	0x00000800U	/* transmit remote fault */
#define	MXFE_TSTAT_APS	0x00000008U	/* autopolarity state */
#define	MXFE_TSTAT_10F	0x00000004U	/* 10Base-T link failure */
#define	MXFE_TSTAT_100F	0x00000002U	/* 100Base-T link failure */
#define	MXFE_ANS_DIS	0x00000000U	/* autonegotiation disabled */
#define	MXFE_ANS_OK	0x00005000U	/* autonegotiation complete */
#define	MXFE_ANS_START	0x00001000U	/* restart autonegotiation */
#define	MXFE_LPC_HDX	0x00200000U 	/* half-duplex */
#define	MXFE_LPC_100FDX	0x01000000U	/* 100 full-duplex */
#define	MXFE_LPC_100HDX	0x00800000U	/* 100 half-duplex */

/* macro to convert TSTAT link partner's code word to MII equivalents */
#define	MXFE_TSTAT_LPAR(x)	((x & MXFE_TSTAT_LPC) >> 16)

/*
 * Bits for SIA reset
 */
#define	MXFE_SIA_RESET	0x00000001U	/* reset 100 PHY */
#define	MXFE_SIA_NRESET	0x00000002U	/* reset NWay */

/*
 * Bits for TCTL
 */
#define	MXFE_TCTL_PAUSE		0x00080000U	/* Pause enable */
#define	MXFE_TCTL_100BT4	0x00040000U	/* 100 BaseT4 enable */
#define	MXFE_TCTL_100FDX	0x00020000U	/* 100 BaseT fdx enable */
#define	MXFE_TCTL_100HDX	0x00010000U	/* 100 BaseT hdx enable */
#define	MXFE_TCTL_LTE	0x00001000U	/* link test enable */
#define	MXFE_TCTL_RSQ	0x00000100U	/* receive squelch enable */
#define	MXFE_TCTL_ANE	0x00000080U	/* autoneg. enable */
#define	MXFE_TCTL_HDX	0x00000040U	/* half-duplex enable */
#define	MXFE_TCTL_PWR	0x00000002U	/* supply power to 10BaseT */

#ifdef	_KERNEL
/*
 * Put exported kernel interfaces here.  (There should be none.)
 */
#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _MXFE_H */
