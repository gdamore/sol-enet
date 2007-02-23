/*
 * Solaris DLPI driver for ethernet cards based on the ADMtek Centaur
 *
 * Copyright (c) 2001-2005 by Garrett D'Amore <garrett@damore.org>.
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

#ifndef	_AFE_H
#define	_AFE_H

#ident	"@(#)$Id: afe.h,v 1.4 2007/02/23 02:56:30 gdamore Exp $"

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
 * via the AFEIOC_GETCSR and friends ioctls.  These are private to this
 * driver and the bundled diagnostic utility, and should not be used by
 * end user application programs.
 */

/*
 * AFE register definitions.
 */
/* PCI configuration registers */
#define	AFE_PCI_VID	0x00	/* Loaded vendor ID */
#define	AFE_PCI_DID	0x02	/* Loaded device ID */
#define	AFE_PCI_COMM	0x04	/* Configuration command register */
#define	AFE_PCI_STAT	0x06	/* Configuration status register */
#define	AFE_PCI_RID	0x08	/* Revision ID */
#define	AFE_PCI_PCC	0x09	/* Programming class code, not used */
#define	AFE_PCI_SCC	0x0a	/* Subclass code, 00 == ethernet */
#define	AFE_PCI_BCC	0x0b	/* Base class code, 02 == network */
#define	AFE_PCI_CLS	0x0c	/* Cache line size */
#define	AFE_PCI_LAT	0x0d	/* Latency timer */
#define	AFE_PCI_IOBA	0x10	/* IO Base Address */
#define	AFE_PCI_MBA	0x14	/* Memory Base Address */
#define	AFE_PCI_CIS	0x28	/* Card Inforamtion Structure (CardBus) */
#define	AFE_PCI_SVID	0x2c	/* Subsystem vendor ID */
#define	AFE_PCI_SSID	0x2e	/* Subsystem ID */
#define	AFE_PCI_BRBA	0x30	/* Boot ROM Base Address (ROM size = 256KB) */
#define	AFE_PCI_CP	0x34	/* Capability Pointer */
#define	AFE_PCI_ILINE	0x3c	/* Interrupt Line */
#define	AFE_PCI_IPIN	0x3d	/* Interrupt Pin */
#define	AFE_PCI_MINGNT	0x3e	/* Minimum Grant */
#define	AFE_PCI_MAXLAT	0x3f	/* Maximum latency */
#define	AFE_PCI_DS	0x40	/* Driver space for special purpose */
#define	AFE_PCI_SIG	0x80	/* Signature of AN983 */
#define	AFE_PCI_PMR0	0xc0	/* Power Management Register 0 */
#define	AFE_PCI_PMR1	0xc4	/* Power Management Register 1 */

/*
 * Bits for PCI command register.
 */
#define	AFE_PCI_BME	0x0004	/* bus master enable */
#define	AFE_PCI_MAE	0x0002	/* memory access enable */
#define	AFE_PCI_IOE	0x0001	/* I/O access enable */

/* Ordinary control/status registers */
#define	AFE_CSR_PAR	0x00	/* PCI access register */
#define	AFE_CSR_TDR	0x08	/* Transmit demand register */
#define	AFE_CSR_RDR	0x10	/* Receive demand register */
#define	AFE_CSR_RDB	0x18	/* Receive descriptor base address */
#define	AFE_CSR_TDB	0x20	/* Transmit descriptor base address */
#define	AFE_CSR_SR	0x28	/* Status register */
#define	AFE_CSR_NAR	0x30	/* Network access register */
#define	AFE_CSR_IER	0x38	/* Interrupt enable register */
#define	AFE_CSR_LPC	0x40	/* Lost packet counter */
#define	AFE_CSR_SPR	0x48	/* Serial port register */
#define	AFE_CSR_TIMER	0x58	/* Timer */
#define	AFE_CSR_SR2	0x80	/* Status register 2 */
#define	AFE_CSR_IER2	0x84	/* Interrupt enable register 2 */
#define	AFE_CSR_CR	0x88	/* Command register */
#define	AFE_CSR_PMCSR	0x90	/* Power Management Command and Status */
#define	AFE_CSR_PAR0	0xa4	/* Physical address register 0 */
#define	AFE_CSR_PAR1	0xa8	/* Physical address register 1 */
#define	AFE_CSR_MAR0	0xac	/* Multicast address hash table register 0 */
#define	AFE_CSR_MAR1	0xb0	/* Multicast address hash table register 1 */
#define	AFE_CSR_BMCR	0xb4	/* PHY BMCR (comet only) */
#define	AFE_CSR_BMSR	0xb8	/* PHY BMSR (comet only) */
#define	AFE_CSR_PHYIDR1	0xbc	/* PHY PHYIDR1 (comet only) */
#define	AFE_CSR_PHYIDR2	0xc0	/* PHY PHYIDR2 (comet only) */
#define	AFE_CSR_ANAR	0xc4	/* PHY ANAR (comet only) */
#define	AFE_CSR_ANLPAR	0xc8	/* PHY ANLPAR (comet only) */
#define	AFE_CSR_ANER	0xcc	/* PHY ANER (comet only) */
#define	AFE_CSR_XMC	0xd0	/* XCVR mode control (comet only) */
#define	AFE_CSR_XCIIS	0xd4	/* XCVR config info/int status (comet only) */
#define	AFE_CSR_XIE	0xd8	/* XCVR interupt enable (comet only) */
#define	AFE_CSR_OPM	0xfc	/* Opmode register (centaur only) */

/*
 * Bits for PCI access register.
 */
#define	AFE_RESET	0x00000001U	/* Reset the entire chip */
#define	AFE_MWIE	0x01000000U	/* PCI memory-write-invalidate */
#define	AFE_MRLE	0x00800000U	/* PCI memory-read-line */
#define	AFE_MRME	0x00200000U	/* PCI memory-read-multiple */
#define	AFE_TXHIPRI	0x00000002U	/* Transmit higher priority */
#define	AFE_DESCSKIP	0x0000007cU	/* Descriptor skip length in DW */
#define	AFE_BIGENDIAN	0x00000080U	/* Use big endian data buffers */
#define	AFE_TXAUTOPOLL	0x00060000U	/* Programmable TX autopoll interval */
#define	AFE_RXFIFO_100	0x00009000U	/* RX FIFO control, Centaur only */
#define	AFE_RXFIFO_10	0x00002800U	/* RX FIFO control, Centaur only */
#define	AFE_CALIGN_NONE	0x00000000U	/* No cache alignment, Comet */
#define	AFE_CALIGN_8	0x00004000U	/* 8 DW cache alignment, Comet */
#define	AFE_CALIGN_16	0x00008000U	/* 16 DW cache alignment, Comet */
#define AFE_CALIGN_32	0x0000c000U	/* 32 DW cache alignment, Comet */
#define	AFE_BURSTLEN	0x00003F00U	/* Programmable burst length, Comet */
#define	AFE_BURSTUNL	0x00000000U	/* Unlimited burst length, Comet */
#define	AFE_BURST_1	0x00000100U	/* 1 DW burst length, Comet */
#define	AFE_BURST_2	0x00000200U	/* 2 DW burst length, Comet */
#define	AFE_BURST_4	0x00000400U	/* 4 DW burst length, Comet */
#define	AFE_BURST_8	0x00000800U	/* 8 DW burst length, Comet */
#define	AFE_BURST_16	0x00001000U	/* 16 DW burst length, Comet */
#define	AFE_BURST_32	0x00002000U	/* 32 DW burst length, Comet */

/*
 * Bits for status register.
 */
#define	AFE_BERR_TYPE		0x03800000U	/* bus error type */
#define	AFE_BERR_PARITY		0x00000000U	/* parity error */
#define	AFE_BERR_TARGET_ABORT	0x01000000U	/* target abort */
#define	AFE_BERR_MASTER_ABORT	0x00800000U	/* master abort */

/*
 * Interrupts.  These are in IER2 and SR2.  Some of them also appear
 * in SR and IER, but we only use the ADMtek specific IER2 and SR2.
 */
#define	AFE_INT_TXEARLY		0x80000000U	/* transmit early interrupt */
#define	AFE_INT_RXEARLY		0x40000000U	/* receive early interrupt */
#define	AFE_INT_LINKCHG		0x20000000U	/* link status changed */
#define	AFE_INT_TXDEFER		0x10000000U	/* transmit defer interrupt */
#define	AFE_INT_PAUSE		0x04000000U	/* pause frame received */
#define	AFE_INT_NORMAL		0x00010000U	/* normal interrupt */
#define	AFE_INT_ABNORMAL	0x00008000U	/* abnormal interrupt */
#define	AFE_INT_BUSERR		0x00002000U	/* fatal bus error */
#define	AFE_INT_TIMER		0x00000800U	/* onboard timer */
#define	AFE_INT_RXJABBER	0x00000200U	/* receive watchdog */
#define	AFE_INT_RXIDLE		0x00000100U	/* receive idle */
#define	AFE_INT_RXNOBUF		0x00000080U	/* no rcv descriptor */
#define	AFE_INT_RXOK		0x00000040U	/* receive complete */
#define	AFE_INT_TXUNDERFLOW	0x00000020U	/* transmit underflow */
#define	AFE_INT_TXJABBER	0x00000008U	/* transmit jabber timeout */
#define	AFE_INT_TXNOBUF		0x00000004U	/* no xmt descriptor */
#define	AFE_INT_TXIDLE		0x00000002U	/* transmit idle interrupt */
#define	AFE_INT_TXOK		0x00000001U	/* transmit ok interrupt */

#define	AFE_INT_ALL		0xf401abefU	/* all above interrupts */
#define	AFE_INT_NONE		0x00000000U	/* no interrupts */
#define	AFE_INT_WANTED		(AFE_INT_NORMAL | AFE_INT_ABNORMAL | \
				AFE_INT_BUSERR | AFE_INT_RXJABBER | \
				AFE_INT_RXNOBUF | AFE_INT_RXIDLE | \
				AFE_INT_RXOK | AFE_INT_TXUNDERFLOW | \
				AFE_INT_LINKCHG | AFE_INT_TXJABBER | \
				AFE_INT_TXOK | AFE_INT_TIMER)

/*
 * Bits for network access register.
 */
#define	AFE_TX_ENABLE	0x00002000U	/* Enable transmit */
#define	AFE_RX_MULTI	0x00000080U	/* Receive all multicast packets */
#define	AFE_RX_PROMISC	0x00000040U	/* Receive any good packet */
#define	AFE_RX_BAD	0x00000008U	/* Pass bad packets */
#define	AFE_RX_ENABLE	0x00000002U	/* Enable receive */
#define	AFE_NAR_TR	0x0000c000U	/* Transmit threshold mask */
#define	AFE_NAR_TR_72	0x00000000U	/* 72 B (128 @ 100Mbps) tx thresh */
#define	AFE_NAR_TR_96	0x00004000U	/* 96 B (256 @ 100Mbps) tx thresh */
#define	AFE_NAR_TR_128	0x00008000U	/* 128 B (512 @ 100Mbps) tx thresh */
#define	AFE_NAR_TR_160	0x0000c000U	/* 160 B (1K @ 100Mbsp) tx thresh */
#define	AFE_NAR_SF	0x00200000U	/* store and forward */
#define	AFE_NAR_HBD	0x00080000U	/* Disable SQE heartbeat */
#define	AFE_NAR_FCOLL	0x00001000U	/* force collision */
#define	AFE_NAR_MODE	0x00000c00U	/* mode (loopback, etc.) */
#define	AFE_NAR_MACLOOP	0x00000400U	/* mac loop back */

/*
 * Bits for lost packet counter.
 */
#define	AFE_LPC_COUNT	0x00007FFFU	/* Count of missed frames */
#define	AFE_LPC_OFLOW	0x00008000U	/* Counter overflow bit */

/*
 * Bits for AFE_CSR_SPR (MII and SROM access)
 */
#define	AFE_MII_DIN	0x00080000U	/* MII data input */
#define	AFE_MII_CONTROL	0x00040000U	/* MII management control, 1=read */
#define	AFE_MII_DOUT	0x00020000U	/* MII data output */
#define	AFE_MII_CLOCK	0x00010000U	/* MII data clock */
#define	AFE_SROM_READ	0x00004000U	/* Serial EEPROM read control */
#define	AFE_SROM_WRITE	0x00002000U	/* Serial EEPROM write control */
#define	AFE_SROM_SEL	0x00000800U	/* Serial EEPROM select */
#define	AFE_SROM_DOUT	0x00000008U	/* Serial EEPROM data out */
#define	AFE_SROM_DIN	0x00000004U	/* Serial EEPROM data in */
#define	AFE_SROM_CLOCK	0x00000002U	/* Serial EEPROM clock */
#define	AFE_SROM_CHIP	0x00000001U	/* Serial EEPROM chip select */
#define	AFE_SROM_ENADDR		0x4	/* Offset of ethernet address */
#define	AFE_SROM_READCMD	0x6	/* command to read SROM */

/*
 * Bits for AFE_CSR_TIMER
 */
#define	AFE_TIMER_LOOP	0x00010000U	/* continuous operating mode */
#define	AFE_TIMER_USEC	204		/* usecs per timer count */

/*
 * Bits for AFE_CSR_CR
 */
#define	AFE_CR_TXURAUTOR	0x1	/* transmit underrun auto recovery */
/*
 * Bits for XMC (Comet specific)
 */
#define	AFE_XMC_LDIS	0x0800		/* long distance 10Base-T cable */

/*
 * Bits for XCIIS (Comet specific)
 */
#define	AFE_XCIIS_SPEED		0x0200	/* 100 Mbps mode */
#define	AFE_XCIIS_DUPLEX	0x0100	/* full duplex mode */
#define	AFE_XCIIS_FLOWCTL	0x0080	/* flow control support */
#define	AFE_XCIIS_ANC		0x0040	/* autonegotiation complete */
#define	AFE_XCIIS_RF		0x0020	/* remote fault detected */
#define	AFE_XCIIS_LFAIL		0x0010	/* link fail */
#define	AFE_XCIIS_ANLPAR	0x0008	/* anar received from link partner */
#define	AFE_XCIIS_PDF		0x0004	/* parallel detection fault */
#define	AFE_XCIIS_ANPR		0x0002	/* autoneg. page received */
#define	AFE_XCIIS_REF		0x0001	/* receive error counter full */

/*
 * Bits for XIE (Comet specific)
 */
#define	AFE_XIE_ANCE		0x0040	/* aneg complete interrupt enable */
#define	AFE_XIE_RFE		0x0020	/* remote fault interrupt enable */
#define	AFE_XIE_LDE		0x0010	/* link fail interrupt enable */
#define	AFE_XIE_ANAE		0x0008	/* aneg. ack. interrupt enable */
#define	AFE_XIE_PDFE		0x0004	/* parallel det. fault int. enable */
#define	AFE_XIE_ANPE		0x0002	/* autoneg. page rec'd int. enable */
#define	AFE_XIE_REFE		0x0001	/* receive error full int. enable */

/*
 * Bits for Opmode (Centaur specific)
 */
#define	AFE_OPM_SPEED	0x80000000U	/* 100 Mbps */
#define	AFE_OPM_DUPLEX	0x40000000U	/* full duplex */
#define	AFE_OPM_LINK	0x20000000U	/* link up? */
#define	AFE_OPM_MODE	0x00000007U	/* mode mask */
#define	AFE_OPM_INTPHY	0x00000007U	/* single chip mode, internal PHY */
#define	AFE_OPM_MACONLY	0x00000004U	/* MAC ony mode, external PHY */

/*
 * Chip extensions to MII.
 */
#define	AFE_PHY_PILR	0x10		/* an983b 1.1 - polarity/int lvl */
#define	AFE_PHY_MCR	0x15		/* an983b 1.1 - mode control */

#define	AFE_PILR_NOSQE	0x0800		/* disable 10BaseT SQE */
#define	AFE_MCR_FIBER	0x0001		/* enable fiber */

#ifdef	_KERNEL
/*
 * Put exported kernel interfaces here.  (There should be none.)
 */
#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _AFE_H */
