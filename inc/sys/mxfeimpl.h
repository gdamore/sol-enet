/*
 * Solaris DLPI driver for ethernet cards based on the Macronix 98715
 *
 * Copyright (c) 2001-2004 by Garrett D'Amore <garrett@damore.org>.
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

#ifndef	_MXFEIMPL_H
#define	_MXFEIMPL_H

#ident	"@(#)$Id: mxfeimpl.h,v 1.2 2004/08/28 06:09:46 gdamore Exp $"

#ifdef	_KERNEL

/*
 * Compile time tunables.
 */
#define	MXFE_MCCHUNK	64	/* # of mc addresses to allocate per chunk */
#define MXFE_NUMBUFS	128	/* # of DMA buffers to allocate */
#define	MXFE_RXRING	32	/* number of xmt buffers */
#define	MXFE_TXRING	32	/* number of rcv buffers */
#define	MXFE_RECLAIM	8	/* low water to reclaim tx buffers */
#define	MXFE_LINKTIMER	5000	/* how often we check link state (in msec) */
#define	MXFE_RSVDBUFS	4	/* how many buffers to reserve */
#define	MXFE_HEADROOM	34	/* headroom in packet (should be 2 modulo 4) */

/*
 * Constants, do not change.
 */
#define	MXFE_BUFSZ	((ETHERMAX + ETHERFCSL + 3) & ~3)
#define	MXFE_ADDRL	(sizeof (u_short) + ETHERADDRL)
#define	MXFE_SETUP_LEN	192		/* size of a setup frame */

/*
 * This entire file is private to the MXFE driver.
 */
#define	MXFE_IDNUM	(0x6163)	/* streams module id number ("ac") */
#define	MXFE_IDNAME	"mxfe"		/* streams module id name */
#define	MXFE_MAXPSZ	(1500)		/* max packet size */
#define	MXFE_MINPSZ	(0)		/* min packet size */
#define	MXFE_HIWAT	(128 * 1024)	/* hi-water mark */
#define	MXFE_LOWAT	(1)		/* lo-water mark */

typedef struct mxfe mxfe_t;
typedef struct mxfe_card mxfe_card_t;
typedef struct mxfe_nd mxfe_nd_t;
typedef struct mxfe_buf mxfe_buf_t;
typedef struct mxfe_desc mxfe_desc_t;
typedef int (*mxfe_nd_pf_t)(mxfe_t *, mblk_t *, mxfe_nd_t *);

struct mxfe_card {
	ushort		card_venid;	/* PCI vendor id */
	ushort		card_devid;	/* PCI device id */
	ushort		card_revid;	/* PCI revision id */
	ushort		card_revmask;
	char		*card_cardname;	/* Description of the card */
	unsigned	card_model;	/* Card specific flags */
};

struct mxfe_nd {
	mxfe_nd_t	*nd_next;
	char		*nd_name;
	mxfe_nd_pf_t	nd_get;
	mxfe_nd_pf_t	nd_set;
	intptr_t	nd_arg1;
	intptr_t	nd_arg2;
};

/*
 * Device instance structure, one per PCI card.
 */
struct mxfe {
	dev_info_t		*mxfe_dip;
	gld_mac_info_t		*mxfe_macinfo;
	mxfe_card_t		*mxfe_cardp;
	ushort			mxfe_cachesize;
	int			mxfe_flags;
	kmutex_t		mxfe_xmtlock;
	kmutex_t		mxfe_intrlock;
	ushort			mxfe_sromwidth;
	int			mxfe_linkstate;
	int			mxfe_lastlinkdown;
	u_longlong_t		mxfe_lastifspeed;
	int			mxfe_lastduplex;
	/*
	 * Buffer management.
	 */
	kmutex_t		mxfe_buflock;
	int			mxfe_numbufs;
	int			mxfe_topbuf;
	struct mxfe_buf		**mxfe_buftab;
	struct mxfe_buf		**mxfe_txbufs;
	struct mxfe_buf		**mxfe_rxbufs;
	/*
	 * Transceiver stuff.
	 */
	int			mxfe_phyaddr;
	int			mxfe_adv_aneg;
	int			mxfe_adv_100T4;
	int			mxfe_adv_100fdx;
	int			mxfe_adv_100hdx;
	int			mxfe_adv_10fdx;
	int			mxfe_adv_10hdx;
	int			mxfe_forcephy;
	int			mxfe_bmsr;
	int			mxfe_anlpar;
	int			mxfe_aner;
	/*
	 * Address management.
	 */
	uchar_t			mxfe_curraddr[ETHERADDRL];
	uchar_t			mxfe_factaddr[ETHERADDRL];
	int			mxfe_promisc;
	/*
	 * Descriptors.
	 */
	int			mxfe_rxring;
	int			mxfe_txring;
	struct mxfe_desc	*mxfe_rxdescp;
	struct mxfe_desc	*mxfe_txdescp;
	int			mxfe_rxcurrent;
	int			mxfe_txreclaim;
	int			mxfe_txsend;
	int			mxfe_txavail;
	/*
	 * Register and DMA access.
	 */
	uchar_t			*mxfe_regs;
	ddi_iblock_cookie_t	mxfe_icookie;
	ddi_acc_handle_t	mxfe_regshandle;
	off_t			mxfe_regsize;
	ddi_acc_handle_t	mxfe_desc_acchandle;
	ddi_dma_handle_t	mxfe_desc_dmahandle;
	uint32_t		mxfe_desc_txpaddr;
	uint32_t		mxfe_desc_rxpaddr;
	caddr_t			mxfe_desc_kaddr;
	/*
	 * NDD related support.
	 */
	mxfe_nd_t		*mxfe_ndp;
	/*
	 * Setup frame related.
	 */
	struct mxfe_buf		**mxfe_temp_bufs;
	ushort_t		mxfe_mcmask;
	ushort_t		mxfe_mccount[512];
	ddi_dma_handle_t	mxfe_setup_descdmah;
	ddi_acc_handle_t	mxfe_setup_descacch;
	uint32_t		mxfe_setup_descpaddr;
	struct mxfe_desc	*mxfe_setup_desc;
	ddi_dma_handle_t	mxfe_setup_bufdmah;
	uint32_t		mxfe_setup_bufpaddr;
	char			mxfe_setup_buf[MXFE_SETUP_LEN];
	/*
	 * Kstats.  Note that these stats were specified internally at
	 * Sun as part of PSARC cases 1997/198, and 1997/247, but these
	 * were never formally published.  Nevertheless, they exist in
	 * the DDK and should be considered semi-public interfaces.  (The
	 * PSARC actually gave them non-private commitment levels, but
	 * failed to follow through by publishing them anywhere.)
	 */
	ulong_t			mxfe_linkup;
	unsigned		mxfe_ifspeed;
	unsigned		mxfe_media;
	unsigned		mxfe_intr;
	unsigned		mxfe_norcvbuf;
	unsigned		mxfe_errrcv;
	unsigned		mxfe_errxmt;
	unsigned		mxfe_missed;
	unsigned		mxfe_underflow;
	unsigned		mxfe_overflow;
	unsigned		mxfe_align_errors;
	unsigned		mxfe_fcs_errors;
	unsigned		mxfe_duplex;
	unsigned		mxfe_carrier_errors;
	unsigned		mxfe_collisions;
	unsigned		mxfe_ex_collisions;
	unsigned		mxfe_tx_late_collisions;
	unsigned		mxfe_defer_xmts;
	unsigned		mxfe_first_collisions;
	unsigned		mxfe_multi_collisions;
	unsigned		mxfe_sqe_errors;
	unsigned		mxfe_macxmt_errors;
	unsigned		mxfe_macrcv_errors;
	unsigned		mxfe_toolong_errors;
	unsigned		mxfe_runt;
};

struct mxfe_buf {
	caddr_t			bp_buf;
	mxfe_t			*bp_mxfep;
	frtn_t			bp_frtn;
	uint32_t		bp_paddr;
	uint32_t		bp_flags;
	int			bp_len;
	ddi_dma_handle_t	bp_dma_handle;
	ddi_acc_handle_t	bp_acc_handle;
};

/*
 * Address structure.
 */
typedef struct mxfe_addr {
	struct ether_addr	dl_phys;
	ushort_t		dl_sap;
} mxfe_addr_t;

/*
 * Descriptor.  We use rings rather than chains.
 */
struct mxfe_desc {
	unsigned	desc_status;
	unsigned	desc_control;
	unsigned	desc_buffer1;
	unsigned	desc_buffer2;
};

#define	PUTDESC(mxfep, member, val)	\
	ddi_put32(mxfep->mxfe_desc_acchandle, &member, val)

#define	GETDESC(mxfep, member)	\
	ddi_get32(mxfep->mxfe_desc_acchandle, &member)

/*
 * Receive descriptor fields.
 */
#define	MXFE_RXSTAT_OWN		0x80000000U	/* ownership */
#define	MXFE_RXSTAT_RXLEN	0x3FFF0000U	/* frame length, incl. crc */
#define	MXFE_RXSTAT_RXERR	0x00008000U	/* error summary */
#define	MXFE_RXSTAT_DESCERR	0x00004000U	/* descriptor error */
#define	MXFE_RXSTAT_RXTYPE	0x00003000U	/* data type */
#define	MXFE_RXSTAT_RUNT		0x00000800U	/* runt frame */
#define	MXFE_RXSTAT_GROUP	0x00000400U	/* multicast/brdcast frame */
#define	MXFE_RXSTAT_FIRST	0x00000200U	/* first descriptor */
#define	MXFE_RXSTAT_LAST		0x00000100U	/* last descriptor */
#define	MXFE_RXSTAT_TOOLONG	0x00000080U	/* frame too long */
#define	MXFE_RXSTAT_COLLSEEN	0x00000040U	/* late collision seen */
#define	MXFE_RXSTAT_FRTYPE	0x00000020U	/* frame type */
#define	MXFE_RXSTAT_WATCHDOG	0x00000010U	/* receive watchdog */
#define	MXFE_RXSTAT_DRIBBLE	0x00000004U	/* dribbling bit */
#define	MXFE_RXSTAT_CRCERR	0x00000002U	/* crc error */
#define	MXFE_RXSTAT_OFLOW	0x00000001U	/* fifo overflow */
#define	MXFE_RXLENGTH(x)		((x & MXFE_RXSTAT_RXLEN) >> 16)

#define	MXFE_RXCTL_ENDRING	0x02000000U	/* end of ring */
#define	MXFE_RXCTL_CHAIN		0x01000000U	/* chained descriptors */
#define	MXFE_RXCTL_BUFLEN2	0x003FF800U	/* buffer 2 length */
#define	MXFE_RXCTL_BUFLEN1	0x000007FFU	/* buffer 1 length */
#define	MXFE_RXBUFLEN1(x)	(x & MXFE_RXCTL_BUFLEN1)
#define	MXFE_RXBUFLEN2(x)	((x & MXFE_RXCTL_BUFLEN2) >> 11)

/*
 * Transmit descriptor fields.
 */
#define	MXFE_TXSTAT_OWN		0x80000000U	/* ownership */
#define	MXFE_TXSTAT_URCNT	0x00C00000U	/* underrun count */
#define	MXFE_TXSTAT_TXERR	0x00008000U	/* error summary */
#define	MXFE_TXSTAT_JABBER	0x00004000U	/* jabber timeout */
#define	MXFE_TXSTAT_CARRLOST	0x00000800U	/* lost carrier */
#define	MXFE_TXSTAT_NOCARR	0x00000400U	/* no carrier */
#define	MXFE_TXSTAT_LATECOL	0x00000200U	/* late collision */
#define	MXFE_TXSTAT_EXCOLL	0x00000100U	/* excessive collisions */
#define	MXFE_TXSTAT_SQE		0x00000080U	/* heartbeat failure */
#define	MXFE_TXSTAT_COLLCNT	0x00000078U	/* collision count */
#define	MXFE_TXSTAT_UFLOW	0x00000002U	/* underflow */
#define	MXFE_TXSTAT_DEFER	0x00000001U	/* deferred */
#define	MXFE_TXCOLLCNT(x)	((x & MXFE_TXSTAT_COLLCNT) >> 3)
#define	MXFE_TXUFLOWCNT(x)	((x & MXFE_TXSTAT_URCNT) >> 22)

#define	MXFE_TXCTL_INTCMPLTE	0x80000000U	/* interrupt completed */
#define	MXFE_TXCTL_LAST		0x40000000U	/* last descriptor */
#define	MXFE_TXCTL_FIRST		0x20000000U	/* first descriptor */
#define	MXFE_TXCTL_NOCRC		0x04000000U	/* disable crc */
#define	MXFE_TXCTL_SETUP		0x08000000U	/* setup frame */
#define	MXFE_TXCTL_ENDRING	0x02000000U	/* end of ring */
#define	MXFE_TXCTL_CHAIN		0x01000000U	/* chained descriptors */
#define	MXFE_TXCTL_NOPAD		0x00800000U	/* disable padding */
#define	MXFE_TXCTL_HASHPERF	0x00400000U	/* hash perfect mode */
#define	MXFE_TXCTL_BUFLEN2	0x003FF800U	/* buffer length 2 */
#define	MXFE_TXCTL_BUFLEN1	0x000007FFU	/* buffer length 1 */
#define	MXFE_TXBUFLEN1(x)	(x & MXFE_TXCTL_BUFLEN1)
#define	MXFE_TXBUFLEN2(x)	((x & MXFE_TXCTL_BUFLEN2) >> 11)

/*
 * Interface flags.
 */
#define	MXFE_RUNNING	0x1	/* chip is initialized */
#define	MXFE_SUSPENDED	0x2	/* interface is suspended */
#define	MXFE_SYMBOL	0x8	/* use symbol mode */

/*
 * Link flags...
 */
#define	MXFE_NOLINK	0x0	/* initial link state, no timer */
#define	MXFE_NWAYCHECK	0x2	/* checking for NWay support */
#define	MXFE_NWAYRENEG	0x3	/* renegotiating NWay mode */
#define	MXFE_GOODLINK	0x4	/* detected link is good */

/*
 * Card models.
 */
#define	MXFE_MODEL(mxfep)	((mxfep)->mxfe_cardp->card_model)
#define	MXFE_MODEL_98715	0x1
#define	MXFE_MODEL_98715A	0x2
#define	MXFE_MODEL_98715AEC	0x3
#define	MXFE_MODEL_98715B	0x4
#define	MXFE_MODEL_98725	0x5
#define	MXFE_MODEL_98713	0x6
#define	MXFE_MODEL_98713A	0x7
#define	MXFE_MODEL_PNICII	0x8

#define	MXFE_HAS_MII		0x00000004	/* MII interface */
#define	MXFE_HAS_NWAY		0x00000008	/* 21143 style NWay */
#define	MXFE_HAS_98713MII	0x00000800	/* 98713 special MII */

#define	MXFE_MCHASH_128		0x02000000	/* use 128-bit hash */
#define	MXFE_MCHASH_64		0x04000000	/* use 64-bit hash */


#define	MXFE_FLAGS_MX98715	(MXFE_HAS_NWAY)
#define	MXFE_FLAGS_MX98713	(MXFE_HAS_MII | MXFE_HAS_98713MII)

/*
 * Register definitions located in mxfe.h exported header file.
 */

/*
 * Macros to simplify hardware access.
 */
#define	GETCSR(mxfep, reg)	\
	ddi_get32(mxfep->mxfe_regshandle, (uint *)(mxfep->mxfe_regs + reg))

#define	GETCSR16(mxfep, reg)	\
	ddi_get16(mxfep->mxfe_regshandle, (ushort *)(mxfep->mxfe_regs + reg))

#define	PUTCSR(mxfep, reg, val)	\
	ddi_put32(mxfep->mxfe_regshandle, (uint *)(mxfep->mxfe_regs + reg), val)

#define	PUTCSR16(mxfep, reg, val)	\
	ddi_put16(mxfep->mxfe_regshandle, (ushort *)(mxfep->mxfe_regs + reg), val)

#define	SETBIT(mxfep, reg, val)	\
	PUTCSR(mxfep, reg, GETCSR(mxfep, reg) | (val))

#define	CLRBIT(mxfep, reg, val)	\
	PUTCSR(mxfep, reg, GETCSR(mxfep, reg) & ~(val))

#define	SYNCDESC(mxfep, desc, who)	\
	(void) ddi_dma_sync(mxfep->mxfe_desc_dmahandle, \
		(off_t)(((caddr_t)desc) - mxfep->mxfe_desc_kaddr), \
		sizeof (mxfe_desc_t), who)

#define	SYNCBUF(bufp, len, who)	\
	if (ddi_dma_sync(bufp->bp_dma_handle, 0, len, who)) \
		mxfe_error(mxfep->mxfe_dip, "failed syncing buffer for DMA");

/*
 * Debugging flags.
 */
#define	MXFE_DWARN	0x0001
#define	MXFE_DINTR	0x0002
#define	MXFE_DWSRV	0x0004
#define	MXFE_DMACID	0x0008
#define	MXFE_DDLPI	0x0010
#define	MXFE_DPHY	0x0020
#define	MXFE_DPCI	0x0040
#define	MXFE_DCHATTY	0x0080
#define	MXFE_DDMA	0x0100
#define	MXFE_DLINK	0x0200
#define	MXFE_DSROM	0x0400
#define	MXFE_DRECV	0x0800
#define	MXFE_DXMIT	0x1000

/*
 * Useful macros, borrowed from strsun.h (somewhat simplified).
 */
#define	DB_BASE(mp)		((mp)->b_datap->db_base)
#define	DB_LIM(mp)		((mp)->b_datap->db_lim)
#define	MBLKL(mp)		((mp)->b_wptr - (mp)->b_rptr)
#define	MBLKSIZE(mp)		(DB_LIM(mp) - DB_BASE(mp))
#define	IOC_CMD(mp)		(*(int *)(mp)->b_rptr)


#ifdef	DEBUG
#define	DBG(lvl, ...)	mxfe_dprintf(mxfep, __func__, lvl, __VA_ARGS__);
#else
#define	DBG(lvl, ...)
#endif

#endif	/* _KERNEL */

#endif	/* _MXFEIMPL_H */
