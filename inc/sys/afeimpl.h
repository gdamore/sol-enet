/*
 * Solaris DLPI driver for ethernet cards based on the ADMtek Centaur
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

#ifndef	_AFEIMPL_H
#define	_AFEIMPL_H

#ident	"@(#)$Id: afeimpl.h,v 1.5 2007/03/29 03:46:13 gdamore Exp $"

#ifdef	_KERNEL

/*
 * Compile time tunables.
 */
#define	AFE_MCCHUNK	64	/* # of mc addresses to allocate per chunk */
#define	AFE_NUMBUFS	128	/* # of DMA buffers to allocate */
#define	AFE_RXRING	32	/* number of rcv buffers */
#define	AFE_TXRING	32	/* number of xmt buffers */
#define	AFE_RECLAIM	8	/* low water to reclaim tx buffers */
#define	AFE_LINKTIMER	10000	/* how often we check link state (in msec) */
#define	AFE_RSVDBUFS	4	/* how many buffers to reserve */
#define	AFE_HEADROOM	34	/* headroom in packet (should be 2 modulo 4) */

/*
 * Constants, do not change.
 */
#define	AFE_BUFSZ	((ETHERMAX + ETHERFCSL + 3) & ~3)
#define	AFE_ADDRL	(sizeof (ushort_t) + ETHERADDRL)
#define	AFE_MCHASH	(64)

/*
 * This entire file is private to the AFE driver.
 */
#define	AFE_IDNUM	(0x6163)	/* streams module id number ("ac") */
#define	AFE_IDNAME	"afe"		/* streams module id name */
#define	AFE_MAXPSZ	(1500)		/* max packet size */
#define	AFE_MINPSZ	(0)		/* min packet size */
#define	AFE_HIWAT	(128 * 1024)	/* hi-water mark */
#define	AFE_LOWAT	(1)		/* lo-water mark */

typedef struct afe afe_t;
typedef struct afe_card afe_card_t;
typedef struct afe_nd afe_nd_t;
typedef struct afe_buf afe_buf_t;
typedef struct afe_desc afe_desc_t;
typedef int (*afe_nd_pf_t)(afe_t *, mblk_t *, afe_nd_t *);

struct afe_card {
	ushort_t	card_venid;	/* PCI vendor id */
	ushort_t	card_devid;	/* PCI device id */
	char		*card_cardname;	/* Description of the card */
	unsigned	card_model;	/* Card specific flags */
};

struct afe_nd {
	afe_nd_t	*nd_next;
	char		*nd_name;
	afe_nd_pf_t	nd_get;
	afe_nd_pf_t	nd_set;
	intptr_t	nd_arg1;
	intptr_t	nd_arg2;
};

/*
 * Device instance structure, one per PCI card.
 */
struct afe {
	dev_info_t		*afe_dip;
	gld_mac_info_t		*afe_macinfo;
	afe_card_t		*afe_cardp;
	ushort_t		afe_cachesize;
	int			afe_flags;
	kmutex_t		afe_xmtlock;
	kmutex_t		afe_intrlock;
	unsigned		afe_linkup;
	ushort_t		afe_sromwidth;
	ushort_t		afe_txthresh;	/* increasing values 0-4 */
	int			afe_lastlinkdown;
	unsigned		afe_lastifspeed;
	int			afe_lastduplex;
	int			afe_forcefiber;
	int			afe_forcephy;
	int			afe_adv_aneg;
	int			afe_adv_100T4;
	int			afe_adv_100fdx;
	int			afe_adv_100hdx;
	int			afe_adv_10fdx;
	int			afe_adv_10hdx;
	/*
	 * Multicast table.
	 */
	ushort_t		afe_mccount[AFE_MCHASH];
	unsigned		afe_mctab[AFE_MCHASH / 32];
	/*
	 * Buffer management.
	 */
	kmutex_t		afe_buflock;
	int			afe_numbufs;
	int			afe_topbuf;
	struct afe_buf		**afe_buftab;
	struct afe_buf		**afe_txbufs;
	struct afe_buf		**afe_rxbufs;
	/*
	 * Transceiver stuff.
	 */
	int			afe_phyaddr;
	/*
	 * Address management.
	 */
	uchar_t			afe_curraddr[ETHERADDRL];
	uchar_t			afe_factaddr[ETHERADDRL];
	int			afe_promisc;
	/*
	 * Descriptors.
	 */
	struct afe_desc		*afe_rxdescp;
	struct afe_desc		*afe_txdescp;
	int			afe_rxcurrent;
	int			afe_txreclaim;
	int			afe_txsend;
	int			afe_txavail;
	/*
	 * Register and DMA access.
	 */
	uchar_t			*afe_regs;
	ddi_acc_handle_t	afe_regshandle;
	off_t			afe_regsize;
	ddi_acc_handle_t	afe_desc_acchandle;
	ddi_dma_handle_t	afe_desc_dmahandle;
	uint32_t		afe_desc_txpaddr;
	uint32_t		afe_desc_rxpaddr;
	caddr_t			afe_desc_kaddr;
	/*
	 * NDD related support.
	 */
	afe_nd_t		*afe_ndp;
	/*
	 * Kstats.  Note that these stats were specified internally at
	 * Sun as part of PSARC cases 1997/198, and 1997/247, but these
	 * were never formally published.  Nevertheless, they exist in
	 * the DDK and should be considered semi-public interfaces.  (The
	 * PSARC actually gave them non-private commitment levels, but
	 * failed to follow through by publishing them anywhere.)
	 */
	kstat_t			*afe_intrstat;
	unsigned		afe_ifspeed;
	unsigned		afe_media;
	unsigned		afe_intr;
	unsigned		afe_norcvbuf;
	unsigned		afe_errrcv;
	unsigned		afe_errxmt;
	unsigned		afe_missed;
	unsigned		afe_underflow;
	unsigned		afe_overflow;

	unsigned		afe_align_errors;
	unsigned		afe_fcs_errors;
	unsigned		afe_duplex;
	unsigned		afe_carrier_errors;
	unsigned		afe_collisions;
	unsigned		afe_ex_collisions;
	unsigned		afe_tx_late_collisions;
	unsigned		afe_defer_xmts;
	unsigned		afe_first_collisions;
	unsigned		afe_multi_collisions;
	unsigned		afe_sqe_errors;
	unsigned		afe_macxmt_errors;
	unsigned		afe_macrcv_errors;
	unsigned		afe_toolong_errors;
	unsigned		afe_runt;
};

struct afe_buf {
	caddr_t			bp_buf;
	afe_t			*bp_afep;
	frtn_t			bp_frtn;
	uint32_t		bp_paddr;
	uint32_t		bp_flags;
	ddi_dma_handle_t	bp_dma_handle;
	ddi_acc_handle_t	bp_acc_handle;
};

/*
 * Descriptor.  We use rings rather than chains.
 */
struct afe_desc {
	unsigned	desc_status;
	unsigned	desc_control;
	unsigned	desc_buffer1;
	unsigned	desc_buffer2;
};

#define	PUTDESC(afep, member, val)	\
	ddi_put32(afep->afe_desc_acchandle, &member, val)

#define	GETDESC(afep, member)	\
	ddi_get32(afep->afe_desc_acchandle, &member)

/*
 * Receive descriptor fields.
 */
#define	AFE_RXSTAT_OWN		0x80000000U	/* ownership */
#define	AFE_RXSTAT_RXLEN	0x3FFF0000U	/* frame length, incl. crc */
#define	AFE_RXSTAT_RXERR	0x00008000U	/* error summary */
#define	AFE_RXSTAT_DESCERR	0x00004000U	/* descriptor error */
#define	AFE_RXSTAT_RXTYPE	0x00003000U	/* data type */
#define	AFE_RXSTAT_RUNT		0x00000800U	/* runt frame */
#define	AFE_RXSTAT_GROUP	0x00000400U	/* multicast/brdcast frame */
#define	AFE_RXSTAT_FIRST	0x00000200U	/* first descriptor */
#define	AFE_RXSTAT_LAST		0x00000100U	/* last descriptor */
#define	AFE_RXSTAT_TOOLONG	0x00000080U	/* frame too long */
#define	AFE_RXSTAT_COLLSEEN	0x00000040U	/* late collision seen */
#define	AFE_RXSTAT_FRTYPE	0x00000020U	/* frame type */
#define	AFE_RXSTAT_WATCHDOG	0x00000010U	/* receive watchdog */
#define	AFE_RXSTAT_DRIBBLE	0x00000004U	/* dribbling bit */
#define	AFE_RXSTAT_CRCERR	0x00000002U	/* crc error */
#define	AFE_RXSTAT_OFLOW	0x00000001U	/* fifo overflow */
#define	AFE_RXLENGTH(x)		((x & AFE_RXSTAT_RXLEN) >> 16)

#define	AFE_RXCTL_ENDRING	0x02000000U	/* end of ring */
#define	AFE_RXCTL_CHAIN		0x01000000U	/* chained descriptors */
#define	AFE_RXCTL_BUFLEN2	0x003FF800U	/* buffer 2 length */
#define	AFE_RXCTL_BUFLEN1	0x000007FFU	/* buffer 1 length */
#define	AFE_RXBUFLEN1(x)	(x & AFE_RXCTL_BUFLEN1)
#define	AFE_RXBUFLEN2(x)	((x & AFE_RXCTL_BUFLEN2) >> 11)

/*
 * Transmit descriptor fields.
 */
#define	AFE_TXSTAT_OWN		0x80000000U	/* ownership */
#define	AFE_TXSTAT_URCNT	0x00C00000U	/* underrun count */
#define	AFE_TXSTAT_TXERR	0x00008000U	/* error summary */
#define	AFE_TXSTAT_JABBER	0x00004000U	/* jabber timeout */
#define	AFE_TXSTAT_CARRLOST	0x00000800U	/* lost carrier */
#define	AFE_TXSTAT_NOCARR	0x00000400U	/* no carrier */
#define	AFE_TXSTAT_LATECOL	0x00000200U	/* late collision */
#define	AFE_TXSTAT_EXCOLL	0x00000100U	/* excessive collisions */
#define	AFE_TXSTAT_SQE		0x00000080U	/* heartbeat failure */
#define	AFE_TXSTAT_COLLCNT	0x00000078U	/* collision count */
#define	AFE_TXSTAT_UFLOW	0x00000002U	/* underflow */
#define	AFE_TXSTAT_DEFER	0x00000001U	/* deferred */
#define	AFE_TXCOLLCNT(x)	((x & AFE_TXSTAT_COLLCNT) >> 3)
#define	AFE_TXUFLOWCNT(x)	((x & AFE_TXSTAT_URCNT) >> 22)

#define	AFE_TXCTL_INTCMPLTE	0x80000000U	/* interrupt completed */
#define	AFE_TXCTL_LAST		0x40000000U	/* last descriptor */
#define	AFE_TXCTL_FIRST		0x20000000U	/* first descriptor */
#define	AFE_TXCTL_NOCRC		0x04000000U	/* disable crc */
#define	AFE_TXCTL_ENDRING	0x02000000U	/* end of ring */
#define	AFE_TXCTL_CHAIN		0x01000000U	/* chained descriptors */
#define	AFE_TXCTL_NOPAD		0x00800000U	/* disable padding */
#define	AFE_TXCTL_HASHPERF	0x00400000U	/* hash perfect mode */
#define	AFE_TXCTL_BUFLEN2	0x003FF800U	/* buffer length 2 */
#define	AFE_TXCTL_BUFLEN1	0x000007FFU	/* buffer length 1 */
#define	AFE_TXBUFLEN1(x)	(x & AFE_TXCTL_BUFLEN1)
#define	AFE_TXBUFLEN2(x)	((x & AFE_TXCTL_BUFLEN2) >> 11)

/*
 * Interface flags.
 */
#define	AFE_RUNNING	0x1	/* chip is initialized */
#define	AFE_SUSPENDED	0x2	/* interface is suspended */
#define	AFE_HASFIBER	0x4	/* internal phy supports fiber (AFE_PHY_MCR) */

/*
 * Card models.
 */
#define	AFE_MODEL_CENTAUR	0x1
#define	AFE_MODEL_COMET		0x2

#define	AFE_MODEL(afep)		((afep)->afe_cardp->card_model)


/*
 * Register definitions located in afe.h exported header file.
 */

/*
 * Macros to simplify hardware access.
 */
#define	GETCSR(afep, reg)	\
	ddi_get32(afep->afe_regshandle, (uint_t *)(afep->afe_regs + reg))

#define	GETCSR16(afep, reg)	\
	ddi_get16(afep->afe_regshandle, (ushort_t *)(afep->afe_regs + reg))

#define	PUTCSR(afep, reg, val)	\
	ddi_put32(afep->afe_regshandle, (uint_t *)(afep->afe_regs + reg), val)

#define	PUTCSR16(afep, reg, val)	\
	ddi_put16(afep->afe_regshandle, (ushort_t *)(afep->afe_regs + reg), val)

#define	SETBIT(afep, reg, val)	PUTCSR(afep, reg, GETCSR(afep, reg) | (val))

#define	CLRBIT(afep, reg, val)	PUTCSR(afep, reg, GETCSR(afep, reg) & ~(val))

#define	SYNCDESC(afep, desc, who)	\
	(void) ddi_dma_sync(afep->afe_desc_dmahandle, \
		(off_t)(((caddr_t)desc) - afep->afe_desc_kaddr), \
		sizeof (afe_desc_t), who)

#define	SYNCBUF(bufp, len, who)	\
	if (ddi_dma_sync(bufp->bp_dma_handle, 0, len, who)) \
		afe_error(afep->afe_dip, "failed syncing buffer for DMA");

/*
 * Debugging flags.
 */
#define	AFE_DWARN	0x0001
#define	AFE_DINTR	0x0002
#define	AFE_DMACID	0x0008
#define	AFE_DPHY	0x0020
#define	AFE_DPCI	0x0040
#define	AFE_DCHATTY	0x0080
#define	AFE_DDMA	0x0100
#define	AFE_DLINK	0x0200
#define	AFE_DSROM	0x0400
#define	AFE_DRECV	0x0800
#define	AFE_DXMIT	0x1000

/*
 * Useful macros, borrowed from strsun.h (somewhat simplified).
 */
#define	DB_BASE(mp)		((mp)->b_datap->db_base)
#define	DB_LIM(mp)		((mp)->b_datap->db_lim)
#define	MBLKL(mp)		((mp)->b_wptr - (mp)->b_rptr)
#define	MBLKSIZE(mp)		(DB_LIM(mp) - DB_BASE(mp))
#define	IOC_CMD(mp)		(*(int *)(mp)->b_rptr)

#ifdef	DEBUG
#define	DBG(lvl, ...)	afe_dprintf(afep, __func__, lvl, __VA_ARGS__);
#else
#define	DBG(lvl, ...)
#endif

#endif	/* _KERNEL */

#endif	/* _AFEIMPL_H */
