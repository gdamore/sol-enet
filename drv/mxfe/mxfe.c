/*
 * Solaris DLPI driver for ethernet cards based on the Macronix 98715
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ident	"@(#)$Id: mxfe.c,v 1.6 2005/11/27 01:10:30 gdamore Exp $"

#include <sys/varargs.h>
#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/stream.h>
#include <sys/cmn_err.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/kmem.h>
#include <sys/gld.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/mii.h>
#include <sys/mxfe.h>
#include <sys/mxfeimpl.h>

/*
 * Driver globals.
 */

/* patchable debug flag */
#ifdef	DEBUG
static unsigned		mxfe_debug = MXFE_DWARN;
#endif

/* table of supported devices */
static mxfe_card_t mxfe_cards[] = {

	/*
	 * Lite-On products
	 */
	{ 0x11ad, 0xc115, 0, 0, "Lite-On LC82C115", MXFE_MODEL_PNICII },

	/*
	 * Macronix chips
	 */
	{ 0x10d9, 0x0531, 0x25, 0xff, "Macronix MX98715AEC",
	  MXFE_MODEL_98715AEC },
	{ 0x10d9, 0x0531, 0x20, 0xff, "Macronix MX98715A", MXFE_MODEL_98715A },
	{ 0x10d9, 0x0531, 0x60, 0xff, "Macronix MX98715B", MXFE_MODEL_98715B },
	{ 0x10d9, 0x0531, 0x30, 0xff, "Macronix MX98725", MXFE_MODEL_98725 },
	{ 0x10d9, 0x0531, 0x00, 0xff, "Macronix MX98715", MXFE_MODEL_98715 },
	{ 0x10d9, 0x0512, 0, 0, "Macronix MX98713", MXFE_MODEL_98713 },

	/*
	 * Compex (relabeled Macronix products)
	 */
	{ 0x11fc, 0x9881, 0x00, 0x00, "Compex 9881", MXFE_MODEL_98713 },
	{ 0x11fc, 0x9881, 0x10, 0xff, "Compex 9881A", MXFE_MODEL_98713A },
	/*
	 * Models listed here
	 */
	{ 0x11ad, 0xc001, 0, 0, "Linksys LNE100TX", MXFE_MODEL_PNICII },
	{ 0x2646, 0x000b, 0, 0, "Kingston KNE111TX", MXFE_MODEL_PNICII },
	{ 0x1154, 0x0308, 0, 0, "Buffalo LGY-PCI-TXL", MXFE_MODEL_98715AEC },
};

/*
 * Function prototypes
 */
static int	mxfe_attach(dev_info_t *, ddi_attach_cmd_t);
static int	mxfe_detach(dev_info_t *, ddi_detach_cmd_t);
static int	mxfe_resume(gld_mac_info_t *);
static int	mxfe_set_mac_addr(gld_mac_info_t *, unsigned char *);
static int	mxfe_set_multicast(gld_mac_info_t *, unsigned char *, int);
static int	mxfe_set_promiscuous(gld_mac_info_t *, int);
static int	mxfe_send(gld_mac_info_t *, mblk_t *);
static int	mxfe_get_stats(gld_mac_info_t *, struct gld_stats *);
static int	mxfe_start(gld_mac_info_t *);
static int	mxfe_stop(gld_mac_info_t *);
static int	mxfe_ioctl(gld_mac_info_t *, queue_t *, mblk_t *);
static unsigned	mxfe_intr(gld_mac_info_t *);
static int	mxfe_reset(gld_mac_info_t *);
static int	mxfe_startmac(mxfe_t *);
static int	mxfe_stopmac(mxfe_t *);
static int	mxfe_mcadd(mxfe_t *, unsigned char *);
static int	mxfe_mcdelete(mxfe_t *, unsigned char *);
static void	mxfe_setrxfilt(mxfe_t *);
static void	mxfe_txreorder(mxfe_t *);
static int	mxfe_allocrings(mxfe_t *);
static void	mxfe_freerings(mxfe_t *);
static int	mxfe_allocsetup(mxfe_t *);
static void	mxfe_freesetup(mxfe_t *);
static mxfe_buf_t	*mxfe_getbuf(mxfe_t *, int);
static void	mxfe_freebuf(mxfe_buf_t *);
static unsigned	mxfe_etherhashle(uchar_t *);
static int	mxfe_msgsize(mblk_t *);
static void	mxfe_miocack(queue_t *, mblk_t *, uint8_t, int, int);
static void	mxfe_error(dev_info_t *, char *, ...);
static void	mxfe_verror(dev_info_t *, int, char *, va_list);
static ushort	mxfe_sromwidth(mxfe_t *);
static ushort	mxfe_readsromword(mxfe_t *, unsigned);
static void	mxfe_readsrom(mxfe_t *, unsigned, unsigned, char *);
static void	mxfe_getfactaddr(mxfe_t *, uchar_t *);
static int	mxfe_miireadbit(mxfe_t *);
static void	mxfe_miiwritebit(mxfe_t *, int);
static void	mxfe_miitristate(mxfe_t *);
static unsigned	mxfe_miiread(mxfe_t *, int, int);
static void	mxfe_miiwrite(mxfe_t *, int, int, ushort);
static unsigned	mxfe_miireadgeneral(mxfe_t *, int, int);
static void	mxfe_miiwritegeneral(mxfe_t *, int, int, ushort);
static unsigned	mxfe_miiread98713(mxfe_t *, int, int);
static void	mxfe_miiwrite98713(mxfe_t *, int, int, ushort);
static void	mxfe_phyinit(mxfe_t *);
static void	mxfe_phyinitmii(mxfe_t *);
static void	mxfe_phyinitnway(mxfe_t *);
static void	mxfe_startnway(mxfe_t *);
static void	mxfe_reportlink(mxfe_t *);
static void	mxfe_checklink(mxfe_t *);
static void	mxfe_checklinkmii(mxfe_t *);
static void	mxfe_checklinknway(mxfe_t *);
static void	mxfe_disableinterrupts(mxfe_t *);
static void	mxfe_enableinterrupts(mxfe_t *);
static void	mxfe_reclaim(mxfe_t *);
static void	mxfe_read(mxfe_t *, int);
static int	mxfe_ndaddbytes(mblk_t *, char *, int);
static int	mxfe_ndaddstr(mblk_t *, char *, int);
static void	mxfe_ndparsestring(mblk_t *, char *, int);
static int	mxfe_ndparselen(mblk_t *);
static int	mxfe_ndparseint(mblk_t *);
static void	mxfe_ndget(mxfe_t *, queue_t *, mblk_t *);
static void	mxfe_ndset(mxfe_t *, queue_t *, mblk_t *);
static void	mxfe_ndfini(mxfe_t *);
static void	mxfe_ndinit(mxfe_t *);

#ifdef	DEBUG
static void	mxfe_dprintf(mxfe_t *, const char *, int, char *, ...);
#endif

/*
 * Stream information
 */
static struct module_info mxfe_module_info = {
	MXFE_IDNUM,		/* mi_idnum */
	MXFE_IDNAME,		/* mi_idname */
	MXFE_MINPSZ,		/* mi_minpsz */
	MXFE_MAXPSZ,		/* mi_maxpsz */
	MXFE_HIWAT,		/* mi_hiwat */
	MXFE_LOWAT		/* mi_lowat */
};

static struct qinit mxfe_rinit = {
	NULL,			/* qi_putp */
	gld_rsrv,		/* qi_srvp */
	gld_open,		/* qi_qopen */
	gld_close,		/* qi_qclose */
	NULL,			/* qi_qadmin */
	&mxfe_module_info,	/* qi_minfo */
	NULL			/* qi_mstat */
};

static struct qinit mxfe_winit = {
	gld_wput,		/* qi_putp */
	gld_wsrv,		/* qi_srvp */
	NULL,			/* qi_qopen */
	NULL,			/* qi_qclose */
	NULL,			/* qi_qadmin */
	&mxfe_module_info,	/* qi_minfo */
	NULL			/* qi_mstat */
};

static struct streamtab mxfe_streamtab = {
	&mxfe_rinit,		/* st_rdinit */
	&mxfe_winit,		/* st_wrinit */
	NULL,			/* st_muxrinit */
	NULL			/* st_muxwinit */
};

/*
 * Character/block operations.
 */
static struct cb_ops mxfe_cbops = {
	nulldev,				/* cb_open */
	nulldev,				/* cb_close */
	nodev,					/* cb_strategy */
	nodev,					/* cb_print */
	nodev,					/* cb_dump */
	nodev,					/* cb_read */
	nodev,					/* cb_write */
	nodev,					/* cb_ioctl */
	nodev,					/* cb_devmap */
	nodev,					/* cb_mmap */
	nodev,					/* cb_segmap */
	nochpoll,				/* cb_chpoll */
	ddi_prop_op,				/* cb_prop_op */
	&mxfe_streamtab,			/* cb_stream */
	D_MP,					/* cb_flag */
	CB_REV,					/* cb_rev */
	nodev,					/* cb_aread */
	nodev					/* cb_awrite */
};

/*
 * Device operations.
 */
static struct dev_ops mxfe_devops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	gld_getinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	mxfe_attach,		/* devo_attach */
	mxfe_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&mxfe_cbops,		/* devo_cb_ops */
	NULL,			/* devo_bus_ops */
	ddi_power		/* devo_power */
};

/*
 * Module linkage information.
 */
#define	MXFE_IDENT	"MXFE Fast Ethernet"
static char mxfe_ident[MODMAXNAMELEN];
static char *mxfe_version;

static struct modldrv mxfe_modldrv = {
	&mod_driverops,			/* drv_modops */
	mxfe_ident,			/* drv_linkinfo */
	&mxfe_devops			/* drv_dev_ops */
};

static struct modlinkage mxfe_modlinkage = {
	MODREV_1,		/* ml_rev */
	{ &mxfe_modldrv, NULL } /* ml_linkage */
};

/*
 * Device attributes.
 */
static ddi_device_acc_attr_t mxfe_devattr = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};

static ddi_device_acc_attr_t mxfe_bufattr = {
	DDI_DEVICE_ATTR_V0,
	DDI_NEVERSWAP_ACC,
	DDI_STRICTORDER_ACC
};

static ddi_dma_attr_t mxfe_dma_attr = {
	DMA_ATTR_V0,		/* dma_attr_version */
	0,			/* dma_attr_addr_lo */
	0xFFFFFFFFU,		/* dma_attr_addr_hi */
	0x7FFFFFFFU,		/* dma_attr_count_max */
	4,			/* dma_attr_align */
	/* FIXME: verify the burstsizes */
	0x3F,			/* dma_attr_burstsizes */
	1,			/* dma_attr_minxfer */
	0xFFFFFFFFU,		/* dma_attr_maxxfer */
	0xFFFFFFFFU,		/* dma_attr_seg */
	1,			/* dma_attr_sgllen */
	1,			/* dma_attr_granular */
	0			/* dma_attr_flags */
};

/*
 * Ethernet addresses.
 */
static uchar_t mxfe_broadcast_addr[ETHERADDRL] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/*
 * DDI entry points.
 */
int
_init(void)
{
	char	*rev = "$Revision: 1.6 $";
	char	*ident = mxfe_ident;

        /* this technique works for both RCS and SCCS */
	strcpy(ident, MXFE_IDENT " v");
	ident += strlen(ident);
	mxfe_version = ident;
	while (*rev) {
		if (strchr("0123456789.", *rev)) {
			*ident = *rev;
			ident++;
			*ident = 0;
		}
		rev++;
	}

	return (mod_install(&mxfe_modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&mxfe_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&mxfe_modlinkage, modinfop));
}

static int
mxfe_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	gld_mac_info_t		*macinfo;
	mxfe_t			*mxfep;
	int			inst = ddi_get_instance(dip);
	ddi_acc_handle_t	pci;
	ushort			venid;
	ushort			devid;
	ushort			revid;
	ushort			svid;
	ushort			ssid;
	ushort			cachesize;
	mxfe_card_t		*cardp;
	int			i;

	switch (cmd) {
	case DDI_RESUME:
		macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
		if (macinfo == NULL) {
			return (DDI_FAILURE);
		}
		return (mxfe_resume(macinfo));

	case DDI_ATTACH:
		break;

	default:
		return (DDI_FAILURE);
	}

	/* this card is a bus master, reject any slave-only slot */
	if (ddi_slaveonly(dip) == DDI_SUCCESS) {
		mxfe_error(dip, "slot does not support PCI bus-master");
		return (DDI_FAILURE);
	}
	/* PCI devices shouldn't generate hilevel interrupts */
	if (ddi_intr_hilevel(dip, 0) != 0) {
		mxfe_error(dip, "hilevel interrupts not supported");
		return (DDI_FAILURE);
	}
	if (pci_config_setup(dip, &pci) != DDI_SUCCESS) {
		mxfe_error(dip, "unable to setup PCI config handle");
		return (DDI_FAILURE);
	}

	venid = pci_config_get16(pci, MXFE_PCI_VID);
	devid = pci_config_get16(pci, MXFE_PCI_DID);
	revid = pci_config_get16(pci, MXFE_PCI_RID);
	svid = pci_config_get16(pci, MXFE_PCI_SVID);
	ssid = pci_config_get16(pci, MXFE_PCI_SSID);

	DBG(MXFE_DPCI, "mingnt %x", pci_config_get8(pci, MXFE_PCI_MINGNT));
	DBG(MXFE_DPCI, "maxlat %x", pci_config_get8(pci, MXFE_PCI_MAXLAT));

	/*
	 * the last entry in the card table matches every possible
	 * card, so the for-loop always terminates properly.
	 */
	cardp = NULL;
	for (i = 0; i < (sizeof (mxfe_cards) / sizeof (mxfe_card_t)); i++) {
		if ((venid == mxfe_cards[i].card_venid) &&
		    (devid == mxfe_cards[i].card_devid) &&
		    ((revid & mxfe_cards[i].card_revmask) ==
			mxfe_cards[i].card_revid)) {
			cardp = &mxfe_cards[i];
		}
		if ((svid == mxfe_cards[i].card_venid) &&
		    (ssid == mxfe_cards[i].card_devid) &&
		    ((revid & mxfe_cards[i].card_revmask) ==
			mxfe_cards[i].card_revid)) {
			cardp = &mxfe_cards[i];
			break;
		}
	}

	if (cardp == NULL) {
		pci_config_teardown(&pci);
		mxfe_error(dip, "Unable to identify PCI card");
		return (DDI_FAILURE);
	}

	if (ddi_prop_update_string(DDI_DEV_T_NONE, dip, "model",
	    cardp->card_cardname) != DDI_SUCCESS) {
		pci_config_teardown(&pci);
		mxfe_error(dip, "Unable to create model property");
		return (DDI_FAILURE);
	}

	/*
	 * Grab the PCI cachesize -- we use this to program the
	 * cache-optimization bus access bits.
	 */
	cachesize = pci_config_get8(pci, MXFE_PCI_CLS);

	if ((macinfo = gld_mac_alloc(dip)) == NULL) {
		pci_config_teardown(&pci);
		mxfe_error(dip, "Unable to allocate macinfo");
		return (DDI_FAILURE);
	}

	/* this cannot fail */
	mxfep = (mxfe_t *)kmem_zalloc(sizeof (mxfe_t), KM_SLEEP);
	mxfep->mxfe_macinfo = macinfo;

	switch (cardp->card_model) {
	case MXFE_MODEL_98715AEC:
	case MXFE_MODEL_PNICII:
		mxfep->mxfe_mcmask = 0x7f;
		break;
	case MXFE_MODEL_98715B:
		mxfep->mxfe_mcmask = 0x3f;
		break;
	default:
		mxfep->mxfe_mcmask = 0x1ff;
		break;
	}

	macinfo->gldm_private =		(void *)mxfep;
	macinfo->gldm_reset =		mxfe_reset;
	macinfo->gldm_start =		mxfe_start;
	macinfo->gldm_stop =		mxfe_stop;
	macinfo->gldm_set_mac_addr =	mxfe_set_mac_addr;
	macinfo->gldm_set_multicast =	mxfe_set_multicast;
	macinfo->gldm_set_promiscuous =	mxfe_set_promiscuous;
	macinfo->gldm_get_stats =	mxfe_get_stats;
	macinfo->gldm_send =		mxfe_send;
	macinfo->gldm_intr =		mxfe_intr;
	macinfo->gldm_ioctl =		mxfe_ioctl;

	macinfo->gldm_ident =		cardp->card_cardname;
	macinfo->gldm_type =		DL_ETHER;
	macinfo->gldm_minpkt =		0;
	macinfo->gldm_maxpkt =		ETHERMTU;
	macinfo->gldm_addrlen =		ETHERADDRL;
	macinfo->gldm_saplen =		-2;
	macinfo->gldm_broadcast_addr =	mxfe_broadcast_addr;
	macinfo->gldm_vendor_addr =	mxfep->mxfe_factaddr;
	macinfo->gldm_devinfo =		dip;
	macinfo->gldm_ppa =		inst;

	/* get the interrupt block cookie */
	if (ddi_get_iblock_cookie(dip, 0, &macinfo->gldm_cookie)
	    != DDI_SUCCESS) {
		mxfe_error(dip, "ddi_get_iblock_cookie failed");
		pci_config_teardown(&pci);
		kmem_free(mxfep, sizeof (mxfe_t));
		gld_mac_free(macinfo);
		return (DDI_FAILURE);
	}

	ddi_set_driver_private(dip, (caddr_t)macinfo);

	mxfep->mxfe_dip = dip;
	mxfep->mxfe_cardp = cardp;
	mxfep->mxfe_phyaddr = -1;
	mxfep->mxfe_cachesize = cachesize;
	mxfep->mxfe_numbufs = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "buffers", MXFE_NUMBUFS);
	mxfep->mxfe_txring = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "txdescriptors", MXFE_TXRING);
	mxfep->mxfe_rxring = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "rxdescriptors", MXFE_RXRING);

        /* default properties */
	mxfep->mxfe_adv_aneg = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "adv_autoneg_cap", 1);
	mxfep->mxfe_adv_100T4 = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "adv_100T4_cap", 1);
	mxfep->mxfe_adv_100fdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "adv_100fdx_cap", 1);
	mxfep->mxfe_adv_100hdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "adv_100hdx_cap", 1);
	mxfep->mxfe_adv_10fdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "adv_10fdx_cap", 1);
	mxfep->mxfe_adv_10hdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
	    "adv_10hdx_cap", 1);

	/*
	 * Legacy properties.  These override newer properties, only
	 * for ease of implementation.
	 */
	switch (ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "speed", 0)) {
	case 100:
		mxfep->mxfe_adv_10fdx = 0;
		mxfep->mxfe_adv_10hdx = 0;
		break;
	case 10:
		mxfep->mxfe_adv_100fdx = 0;
		mxfep->mxfe_adv_100hdx = 0;
		break;
	}

	switch (ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "full-duplex", -1)) {
	case 1:
		mxfep->mxfe_adv_10hdx = 0;
		mxfep->mxfe_adv_100hdx = 0;
		break;
	case 0:
		mxfep->mxfe_adv_10fdx = 0;
		mxfep->mxfe_adv_100fdx = 0;
		break;
	}

	DBG(MXFE_DPCI, "PCI vendor id = %x", venid);
	DBG(MXFE_DPCI, "PCI device id = %x", devid);
	DBG(MXFE_DPCI, "PCI revision id = %x", revid);
	DBG(MXFE_DPCI, "PCI cachesize = %d", cachesize);
	DBG(MXFE_DPCI, "PCI COMM = %x", pci_config_get8(pci, MXFE_PCI_COMM));
	DBG(MXFE_DPCI, "PCI STAT = %x", pci_config_get8(pci, MXFE_PCI_STAT));

	(void) mutex_init(&mxfep->mxfe_buflock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);
	(void) mutex_init(&mxfep->mxfe_xmtlock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);
	(void) mutex_init(&mxfep->mxfe_intrlock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);

	mxfe_ndinit(mxfep);

	/*
	 * Enable bus master, IO space, and memory space accesses.
	 */
	pci_config_put16(pci, MXFE_PCI_COMM,
	    pci_config_get16(pci, MXFE_PCI_COMM) |
	    MXFE_PCI_BME | MXFE_PCI_MAE | MXFE_PCI_IOE);

	/* we're done with this now, drop it */
	pci_config_teardown(&pci);

	/*
	 * Map in the device registers.
	 */
	ddi_dev_regsize(dip, 1, &mxfep->mxfe_regsize);
	if (ddi_regs_map_setup(dip, 1, (caddr_t *)&mxfep->mxfe_regs,
	    0, 0, &mxfe_devattr, &mxfep->mxfe_regshandle)) {
		mxfe_error(dip, "ddi_regs_map_setup failed");
		pci_config_teardown(&pci);
		goto failed;
	}

	/* Stop the board. */
	CLRBIT(mxfep, MXFE_CSR_NAR, MXFE_TX_ENABLE | MXFE_RX_ENABLE);

	/* Turn off all interrupts for now. */
	mxfe_disableinterrupts(mxfep);

	/*
	 * Allocate DMA resources (descriptor rings and buffers).
	 */
	if (mxfe_allocrings(mxfep) != DDI_SUCCESS) {
		mxfe_error(dip, "unable to allocate DMA resources");
		goto failed;
	}

	/*
	 * Allocate resources for setup frame.
	 */
	if (mxfe_allocsetup(mxfep) != DDI_SUCCESS) {
		mxfe_error(dip, "unable to allocate DMA for setup frame");
		goto failed;
	}

	/* Reset the chip. */
	mxfe_reset(macinfo);

	/* FIXME: insert hardware initializations here */

	/* Determine the number of address bits to our EEPROM. */
	mxfep->mxfe_sromwidth = mxfe_sromwidth(mxfep);

	/*
	 * Get the factory ethernet address.  This becomes the current
	 * ethernet address (it can be overridden later via ifconfig).
	 * FIXME: consider allowing this to be tunable via a property
	 */
	mxfe_getfactaddr(mxfep, mxfep->mxfe_factaddr);
	mxfep->mxfe_promisc = GLD_MAC_PROMISC_NONE;

	if (ddi_add_intr(dip, 0, NULL, NULL, gld_intr,
	    (void *)macinfo) != DDI_SUCCESS) {
		mxfe_error(dip, "unable to add interrupt");
		goto failed;
	}

	/* FIXME: do the power management stuff */

	/* make sure we add broadcast address to filter */
	mxfe_set_multicast(macinfo, mxfe_broadcast_addr, GLD_MULTI_ENABLE);

	if (gld_register(dip, MXFE_IDNAME, macinfo) == DDI_SUCCESS) {
		return (DDI_SUCCESS);
	}

	/* failed to register with GLD */
failed:
	if (macinfo->gldm_cookie != NULL) {
		ddi_remove_intr(dip, 0, macinfo->gldm_cookie);
	}
	mxfe_ndfini(mxfep);
	mutex_destroy(&mxfep->mxfe_buflock);
	mutex_destroy(&mxfep->mxfe_intrlock);
	mutex_destroy(&mxfep->mxfe_xmtlock);

	mxfe_freesetup(mxfep);
	mxfe_freerings(mxfep);

	if (mxfep->mxfe_regshandle != NULL) {
		ddi_regs_map_free(&mxfep->mxfe_regshandle);
	}
	kmem_free(mxfep, sizeof (mxfe_t));
	gld_mac_free(macinfo);
	return (DDI_FAILURE);
}

static int
mxfe_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	gld_mac_info_t	*macinfo;
	mxfe_t		*mxfep;
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	if (macinfo == NULL) {
		mxfe_error(dip, "no soft state in detach!");
		return (DDI_FAILURE);
	}
	mxfep = (mxfe_t *)macinfo->gldm_private;

	switch (cmd) {
	case DDI_DETACH:
		if (gld_unregister(macinfo) != DDI_SUCCESS) {
			return (DDI_FAILURE);
		}
		/* make sure hardware is quiesced */
		mxfe_stop(macinfo);

		/* clean up and shut down device */
		ddi_remove_intr(dip, 0, macinfo->gldm_cookie);

		/* FIXME: delete properties */

		/* free up any left over buffers or DMA resources */
		if (mxfep->mxfe_desc_dmahandle != NULL) {
			int	i;
			/* free up buffers first (reclaim) */
			for (i = 0; i < mxfep->mxfe_txring; i++) {
				if (mxfep->mxfe_txbufs[i]) {
					mxfe_freebuf(mxfep->mxfe_txbufs[i]);
					mxfep->mxfe_txbufs[i] = NULL;
				}
			}
			for (i = 0; i < mxfep->mxfe_rxring; i++) {
				if (mxfep->mxfe_rxbufs[i]) {
					mxfe_freebuf(mxfep->mxfe_rxbufs[i]);
					mxfep->mxfe_rxbufs[i] = NULL;
				}
			}
			/* then free up DMA resourcess */
			mxfe_freerings(mxfep);
		}
		mxfe_freesetup(mxfep);

		mxfe_ndfini(mxfep);
		ddi_regs_map_free(&mxfep->mxfe_regshandle);
		mutex_destroy(&mxfep->mxfe_buflock);
		mutex_destroy(&mxfep->mxfe_intrlock);
		mutex_destroy(&mxfep->mxfe_xmtlock);

		kmem_free(mxfep, sizeof (mxfe_t));
		gld_mac_free(macinfo);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		/* quiesce the hardware */
		mutex_enter(&mxfep->mxfe_intrlock);
		mutex_enter(&mxfep->mxfe_xmtlock);
		mxfep->mxfe_flags |= MXFE_SUSPENDED;
		mxfe_stopmac(mxfep);
		mutex_exit(&mxfep->mxfe_xmtlock);
		mutex_exit(&mxfep->mxfe_intrlock);
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
mxfe_ioctl(gld_mac_info_t *macinfo, queue_t *wq, mblk_t *mp)
{
	mxfe_t *mxfep = (mxfe_t *)macinfo->gldm_private;

	switch (IOC_CMD(mp)) {

	case NDIOC_GET:
		mxfe_ndget(mxfep, wq, mp);
		break;

	case NDIOC_SET:
		mxfe_ndset(mxfep, wq, mp);
		break;

	default:
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		break;
	}
	return (GLD_SUCCESS);
}

static int
mxfe_mcadd(mxfe_t *mxfep, unsigned char *macaddr)
{
	unsigned	hash;
	int		changed = 0;
	int		byte;
	int		offset;

	hash =  mxfe_etherhashle(macaddr) & mxfep->mxfe_mcmask;;
	/* calculate the byte offset first */
	byte = hash / 8;
	/* note that this is *not* "byte * 2", due to truncation! */
	offset = ((byte / 2) * 4) + (byte % 2);

	if ((mxfep->mxfe_mccount[hash]) == 0) {
		mxfep->mxfe_setup_buf[offset] |= (1 << (hash % 8));
		changed++;
	}
	mxfep->mxfe_mccount[hash]++;
	return (changed);
}

static int
mxfe_mcdelete(mxfe_t *mxfep, unsigned char *macaddr)
{
	unsigned	hash;
	int		changed = 0;
	int		byte;
	int		offset;

	hash = mxfe_etherhashle(macaddr) & mxfep->mxfe_mcmask;
	/* calculate the byte offset first */
	byte = hash / 8;
	/* note that this is *not* "byte * 2", due to truncation! */
	offset = ((byte / 2) * 4) + (byte % 2);

	if ((mxfep->mxfe_mccount[hash]) == 1) {
		mxfep->mxfe_setup_buf[offset] &= ~(1 << (hash % 8));
		changed++;
	}
	mxfep->mxfe_mccount[hash]--;
	return (changed);
}

static int
mxfe_set_multicast(gld_mac_info_t *macinfo, unsigned char *macaddr, int flag)
{
	mxfe_t			*mxfep = (mxfe_t *)macinfo->gldm_private;
	int			changed = 0;

	/* exclusive access to the card while we reprogram it */
	mutex_enter(&mxfep->mxfe_xmtlock);

	switch (flag) {
	case GLD_MULTI_ENABLE:
		changed = mxfe_mcadd(mxfep, macaddr);
		break;

	case GLD_MULTI_DISABLE:
		changed = mxfe_mcdelete(mxfep, macaddr);
		break;
	}

	if (changed) {
		mxfe_setrxfilt(mxfep);
	}
	mutex_exit(&mxfep->mxfe_xmtlock);

	return (GLD_SUCCESS);
}

static int
mxfe_set_promiscuous(gld_mac_info_t *macinfo, int flag)
{
	mxfe_t		*mxfep = (mxfe_t *)macinfo->gldm_private;

	/* exclusive access to the card while we reprogram it */
	mutex_enter(&mxfep->mxfe_xmtlock);
	/* save current promiscuous mode state for replay in resume */
	mxfep->mxfe_promisc = flag;

	mxfe_setrxfilt(mxfep);

	mutex_exit(&mxfep->mxfe_xmtlock);

	return (GLD_SUCCESS);
}

static int
mxfe_set_mac_addr(gld_mac_info_t *macinfo, unsigned char *macaddr)
{
	mxfe_t		*mxfep = (mxfe_t *)macinfo->gldm_private;
	caddr_t		kaddr = mxfep->mxfe_setup_buf;

	mutex_enter(&mxfep->mxfe_xmtlock);
	bcopy(macaddr, mxfep->mxfe_curraddr, ETHERADDRL);

	kaddr[156] = mxfep->mxfe_curraddr[0];
	kaddr[157] = mxfep->mxfe_curraddr[1];
	kaddr[160] = mxfep->mxfe_curraddr[2];
	kaddr[161] = mxfep->mxfe_curraddr[3];
	kaddr[164] = mxfep->mxfe_curraddr[4];
	kaddr[165] = mxfep->mxfe_curraddr[5];

	mxfe_setrxfilt(mxfep);

	mutex_exit(&mxfep->mxfe_xmtlock);

	return (GLD_SUCCESS);
}

/*
 * Hardware management.
 */
static int
mxfe_resetmac(mxfe_t *mxfep)
{
	int		i;
	unsigned	val;

	ASSERT(mutex_owned(&mxfep->mxfe_intrlock));
	ASSERT(mutex_owned(&mxfep->mxfe_xmtlock));

	DBG(MXFE_DCHATTY, "resetting!");
	SETBIT(mxfep, MXFE_CSR_PAR, MXFE_RESET);
	for (i = 1; i < 10; i++) {
		drv_usecwait(5);
		val = GETCSR(mxfep, MXFE_CSR_PAR);
		if (!(val & MXFE_RESET)) {
			break;
		}
	}
	if (i == 10) {
		mxfe_error(mxfep->mxfe_dip, "timed out waiting for reset!");
		return (GLD_FAILURE);
	}

	/* FIXME: possibly set some other regs here, e.g. arbitration. */
	/* initialize busctl register */

	/* clear all the cache alignment bits */
	CLRBIT(mxfep, MXFE_CSR_PAR, MXFE_CALIGN_32);

	/* then set the cache alignment if its supported */
	switch (mxfep->mxfe_cachesize) {
	case 8:
		SETBIT(mxfep, MXFE_CSR_PAR, MXFE_CALIGN_8);
		break;
	case 16:
		SETBIT(mxfep, MXFE_CSR_PAR, MXFE_CALIGN_16);
		break;
	case 32:
		SETBIT(mxfep, MXFE_CSR_PAR, MXFE_CALIGN_32);
		break;
	}

	/* unconditional 32-word burst */
	SETBIT(mxfep, MXFE_CSR_PAR, MXFE_BURST_32);

	return (GLD_SUCCESS);
}

static int
mxfe_reset(gld_mac_info_t *macinfo)
{
	mxfe_t		*mxfep = (mxfe_t *)macinfo->gldm_private;
	int		rv;

	mutex_enter(&mxfep->mxfe_intrlock);
	mutex_enter(&mxfep->mxfe_xmtlock);

	rv = mxfe_resetmac(mxfep);

	mutex_exit(&mxfep->mxfe_xmtlock);
	mutex_exit(&mxfep->mxfe_intrlock);
	return (rv);

}

static int
mxfe_resume(gld_mac_info_t *macinfo)
{
	mxfe_t	*mxfep;
	if (macinfo == NULL) {
		return (DDI_FAILURE);
	}
	mxfep = (mxfe_t *)macinfo->gldm_private;

	mutex_enter(&mxfep->mxfe_intrlock);
	mutex_enter(&mxfep->mxfe_xmtlock);

	/* reset chip */
	if (mxfe_resetmac(mxfep) != GLD_SUCCESS) {
		mxfe_error(mxfep->mxfe_dip, "unable to resume chip!");
		mxfep->mxfe_flags |= MXFE_SUSPENDED;
		mutex_exit(&mxfep->mxfe_intrlock);
		mutex_exit(&mxfep->mxfe_xmtlock);
		return (DDI_SUCCESS);
	}

	/* restore rx filter */
	mxfe_setrxfilt(mxfep);

	/* start the chip */
	if (mxfep->mxfe_flags & MXFE_RUNNING) {
		if (mxfe_startmac(mxfep) != GLD_SUCCESS) {
			mxfe_error(mxfep->mxfe_dip, "unable to restart mac!");
			mxfep->mxfe_flags |= MXFE_SUSPENDED;
			mutex_exit(&mxfep->mxfe_intrlock);
			mutex_exit(&mxfep->mxfe_xmtlock);
			return (DDI_SUCCESS);
		}
	}

	/* drop locks */
	mutex_exit(&mxfep->mxfe_xmtlock);
	mutex_exit(&mxfep->mxfe_intrlock);

	return (DDI_SUCCESS);
}

/*
 * Serial EEPROM access - derived from the FreeBSD implementation.
 */

static ushort
mxfe_sromwidth(mxfe_t *mxfep)
{
	int		i;
	int		eeread;
	int		addrlen = 8;

 	eeread = MXFE_SROM_READ | MXFE_SROM_SEL | MXFE_SROM_CHIP;

	PUTCSR(mxfep, MXFE_CSR_SPR, eeread & ~MXFE_SROM_CHIP);
	drv_usecwait(1);
	PUTCSR(mxfep, MXFE_CSR_SPR, eeread);

	/* command bits first */
	for (i = 4; i != 0; i >>= 1) {
		unsigned val = (MXFE_SROM_READCMD & i) ? MXFE_SROM_DIN : 0;
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread | val);
		drv_usecwait(1);
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread | val | MXFE_SROM_CLOCK);
		drv_usecwait(1);
	}

	PUTCSR(mxfep, MXFE_CSR_SPR, eeread);

	for (addrlen = 1; addrlen <= 12; addrlen++) {
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread | MXFE_SROM_CLOCK);
		drv_usecwait(1);
		if (!(GETCSR(mxfep, MXFE_CSR_SPR) & MXFE_SROM_DOUT)) {
			PUTCSR(mxfep, MXFE_CSR_SPR, eeread);
			drv_usecwait(1);
			break;
		}
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread);
		drv_usecwait(1);
	}

	/* turn off accesses to the EEPROM */
	PUTCSR(mxfep, MXFE_CSR_SPR, eeread &~ MXFE_SROM_CHIP);

	DBG(MXFE_DSROM, "detected srom width = %d bits", addrlen);

	return ((addrlen < 4 || addrlen > 12) ? 6 : addrlen);
}

/*
 * The words in EEPROM are stored in little endian order.  We
 * shift bits out in big endian order, though.  This requires
 * a byte swap on some platforms.
 */
static ushort
mxfe_readsromword(mxfe_t *mxfep, unsigned romaddr)
{
	int		i;
	ushort		word = 0;
	ushort		retval;
	int		eeread;
	int		addrlen;
	int		readcmd;
	uchar_t		*ptr;

 	eeread = MXFE_SROM_READ | MXFE_SROM_SEL | MXFE_SROM_CHIP;
	addrlen = mxfep->mxfe_sromwidth;
	readcmd = (MXFE_SROM_READCMD << addrlen) | romaddr;

	if (romaddr >= (1 << addrlen)) {
		/* too big to fit! */
		return (0);
	}

	PUTCSR(mxfep, MXFE_CSR_SPR, eeread & ~MXFE_SROM_CHIP);
	PUTCSR(mxfep, MXFE_CSR_SPR, eeread);

	/* command and address bits */
	for (i = 4 + addrlen; i >= 0; i--) {
		short val = (readcmd & (1 << i)) ?  MXFE_SROM_DIN : 0;
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread | val);
		drv_usecwait(1);
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread | val | MXFE_SROM_CLOCK);
		drv_usecwait(1);
	}

	PUTCSR(mxfep, MXFE_CSR_SPR, eeread);

	for (i = 0; i < 16; i++) {
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread | MXFE_SROM_CLOCK);
		drv_usecwait(1);
		word <<= 1;
		if (GETCSR(mxfep, MXFE_CSR_SPR) & MXFE_SROM_DOUT) {
			word |= 1;
		}
		PUTCSR(mxfep, MXFE_CSR_SPR, eeread);
		drv_usecwait(1);
	}

	/* turn off accesses to the EEPROM */
	PUTCSR(mxfep, MXFE_CSR_SPR, eeread &~ MXFE_SROM_CHIP);

	/*
	 * Fix up the endianness thing.  Note that the values
	 * are stored in little endian format on the SROM.
	 */
	DBG(MXFE_DSROM, "got value %d from SROM (before swap)", word);
	ptr = (uchar_t *)&word;
	retval = (ptr[1] << 8) | ptr[0];
	return (retval);
}

static void
mxfe_readsrom(mxfe_t *mxfep, unsigned romaddr, unsigned len, char *dest)
{
	int	i;
	ushort	word;
	ushort	*ptr = (ushort *)dest;
	for (i = 0; i < len; i++) {
		word = mxfe_readsromword(mxfep, romaddr + i);
		*ptr = word;
		DBG(MXFE_DSROM, "word at %d is 0x%x", romaddr + i, word);
		ptr++;
	}
}

static void
mxfe_getfactaddr(mxfe_t *mxfep, uchar_t *eaddr)
{
	ushort	word;
	uchar_t	*ptr;
	word = mxfe_readsromword(mxfep, MXFE_SROM_ENADDR / 2);
	ptr = (uchar_t *)&word;
	word = (ptr[1] << 8) | ptr[0];
	mxfe_readsrom(mxfep, word / 2, ETHERADDRL / 2, (char *)eaddr);
	DBG(MXFE_DMACID,
	    "factory ethernet address = %02x:%02x:%02x:%02x:%02x:%02x",
	    eaddr[0], eaddr[1], eaddr[2], eaddr[3], eaddr[4], eaddr[5]);
}

static void
mxfe_phyinit(mxfe_t *mxfep)
{
	switch (MXFE_MODEL(mxfep)) {
	case MXFE_MODEL_98713A:
		mxfe_phyinitmii(mxfep);
		break;
	default:
		mxfe_phyinitnway(mxfep);
		break;
	}
}

/*
 * NWay support.
 */
static void
mxfe_startnway(mxfe_t *mxfep)
{
	unsigned	nar;
	unsigned	tctl;
	unsigned	restart;

	/* this should not happen in a healthy system */
	if (mxfep->mxfe_linkstate != MXFE_NOLINK) {
		DBG(MXFE_DWARN, "link start called out of state (%x)",
		    mxfep->mxfe_linkstate);
		return;
	}

	if (mxfep->mxfe_adv_aneg == 0) {
		/* not done for forced mode */
		return;
	}

	nar = GETCSR(mxfep, MXFE_CSR_NAR);

	restart = nar & (MXFE_TX_ENABLE | MXFE_RX_ENABLE);

	/* enable scrambler mode - also disables tx/rx */
	PUTCSR(mxfep, MXFE_CSR_NAR, MXFE_NAR_SCR);

	nar = MXFE_NAR_SCR | MXFE_NAR_PCS | MXFE_NAR_HBD;
	tctl = GETCSR(mxfep, MXFE_CSR_TCTL);
	tctl &= ~(MXFE_TCTL_100FDX | MXFE_TCTL_100HDX | MXFE_TCTL_HDX);

	if (mxfep->mxfe_adv_100fdx) {
		tctl |= MXFE_TCTL_100FDX;
	}
	if (mxfep->mxfe_adv_100hdx) {
		tctl |= MXFE_TCTL_100HDX;
	}
	if (mxfep->mxfe_adv_10fdx) {
		nar |= MXFE_NAR_FDX | MXFE_TCTL_PWR;
	}
	if (mxfep->mxfe_adv_10hdx) {
		tctl |= MXFE_TCTL_HDX | MXFE_TCTL_PWR;
	}
	tctl |= MXFE_TCTL_ANE;
	tctl |= MXFE_TCTL_LTE | MXFE_TCTL_RSQ | MXFE_TCTL_PWR;

	/* FIXME: possibly add store-and-forward */
	nar = MXFE_NAR_SCR | MXFE_NAR_PCS | MXFE_NAR_HBD | MXFE_NAR_FDX;

	/* FIXME: possibly this should be absolute, are reads of TCTL sane? */
	/* possibly we should add in support for PAUSE frames */
	DBG(MXFE_DPHY, "writing nar = 0x%x", nar);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar);

	DBG(MXFE_DPHY, "writing tctl = 0x%x", tctl);
	PUTCSR(mxfep, MXFE_CSR_TCTL, tctl);

	/* restart autonegotation */
	DBG(MXFE_DPHY, "writing tstat = 0x%x", MXFE_ANS_START);
	PUTCSR(mxfep, MXFE_CSR_TSTAT, MXFE_ANS_START);

	/* restart tx/rx processes... */
	PUTCSR(mxfep, MXFE_CSR_NAR, nar | restart);

	/* Macronix initializations from Bolo Tsai */
	PUTCSR(mxfep, MXFE_CSR_MXMAGIC, 0x0b2c0000);
	PUTCSR(mxfep, MXFE_CSR_ACOMP, 0x11000);

	mxfep->mxfe_linkstate = MXFE_NWAYCHECK;
}

static void
mxfe_checklinknway(mxfe_t *mxfep)
{
	unsigned	nar, tstat, tctl, lpar;
	int		nwayok = 0;

	DBG(MXFE_DPHY, "NWay check, state %x", mxfep->mxfe_linkstate);
	nar = GETCSR(mxfep, MXFE_CSR_NAR);
	tstat = GETCSR(mxfep, MXFE_CSR_TSTAT);
	tctl = GETCSR(mxfep, MXFE_CSR_TCTL);
	lpar = MXFE_TSTAT_LPAR(tstat);

	mxfep->mxfe_anlpar = lpar;
	if (tstat & MXFE_TSTAT_LPN) {
		mxfep->mxfe_aner |= MII_ANER_LPANA;
	} else {
		mxfep->mxfe_aner &= ~(MII_ANER_LPANA);
	}

	DBG(MXFE_DPHY, "nar(CSR6) = 0x%x", nar);
	DBG(MXFE_DPHY, "tstat(CSR12) = 0x%x", tstat);
	DBG(MXFE_DPHY, "tctl(CSR14) = 0x%x", tctl);
	DBG(MXFE_DPHY, "ANEG state = 0x%x", (tstat & MXFE_TSTAT_ANS) >> 12);

	if ((tstat & MXFE_TSTAT_ANS) != MXFE_ANS_OK) {
		/* autoneg did not complete */
		nwayok = 0;
		mxfep->mxfe_bmsr &= ~MII_BMSR_ANC;
	} else {
		nwayok = 1;
		mxfep->mxfe_bmsr |= ~MII_BMSR_ANC;
	}

	if ((tstat & MXFE_TSTAT_100F) && (tstat & MXFE_TSTAT_10F)) {
		mxfep->mxfe_linkup = 0;
		mxfep->mxfe_ifspeed = 0;
		mxfep->mxfe_duplex = GLD_DUPLEX_UNKNOWN;
		mxfep->mxfe_media = GLDM_UNKNOWN;
		mxfep->mxfe_linkstate = MXFE_NOLINK;
		mxfe_reportlink(mxfep);
		mxfe_startnway(mxfep);
		return;
	}

	/*
	 * if the link is newly up, then we might need to set various
	 * mode bits, or negotiate for parameters, etc.
	 */
	if (mxfep->mxfe_adv_aneg) {

		mxfep->mxfe_media = GLDM_TP;
		mxfep->mxfe_linkup = 1;

		if (tstat & MXFE_TSTAT_LPN) {
			/* partner has NWay */

			if ((mxfep->mxfe_anlpar & MII_ANEG_100FDX) &&
			    mxfep->mxfe_adv_100fdx) {
				mxfep->mxfe_ifspeed = 100000000;
				mxfep->mxfe_duplex = GLD_DUPLEX_FULL;
			} else if ((mxfep->mxfe_anlpar & MII_ANEG_100HDX) &&
			    mxfep->mxfe_adv_100hdx) {
				mxfep->mxfe_ifspeed = 100000000;
				mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
			} else if ((mxfep->mxfe_anlpar & MII_ANEG_10FDX) &&
			    mxfep->mxfe_adv_10fdx) {
				mxfep->mxfe_ifspeed = 10000000;
				mxfep->mxfe_duplex = GLD_DUPLEX_FULL;
			} else if ((mxfep->mxfe_anlpar & MII_ANEG_10HDX) &&
			    mxfep->mxfe_adv_10hdx) {
				mxfep->mxfe_ifspeed = 10000000;
				mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
			} else {
				mxfep->mxfe_media = GLDM_UNKNOWN;
				mxfep->mxfe_ifspeed = 0;
			}
		} else {
			/* link partner does not have NWay */
			/* we just assume half duplex, since we can't
			 * detect it! */
			mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
			if (!(tstat & MXFE_TSTAT_100F)) {
				DBG(MXFE_DPHY, "Partner doesn't have NWAY");
				mxfep->mxfe_ifspeed = 100000000;
			} else {
				mxfep->mxfe_ifspeed = 10000000;
			}
		}
	} else {
		/* forced modes */
		mxfep->mxfe_media = GLDM_TP;
		mxfep->mxfe_linkup = 1;
		if (mxfep->mxfe_adv_100fdx) {
			mxfep->mxfe_ifspeed = 100000000;
			mxfep->mxfe_duplex = GLD_DUPLEX_FULL;
		} else if (mxfep->mxfe_adv_100hdx) {
			mxfep->mxfe_ifspeed = 100000000;
			mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
		} else if (mxfep->mxfe_adv_10fdx) {
			mxfep->mxfe_ifspeed = 10000000;
			mxfep->mxfe_duplex = GLD_DUPLEX_FULL;
		} else if (mxfep->mxfe_adv_10hdx) {
			mxfep->mxfe_ifspeed = 10000000;
			mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
		} else {
			mxfep->mxfe_ifspeed = 0;
		}
	}
	mxfe_reportlink(mxfep);
	mxfep->mxfe_linkstate = MXFE_GOODLINK;
}

static void
mxfe_phyinitnway(mxfe_t *mxfep)
{
	mxfep->mxfe_linkstate = MXFE_NOLINK;
	mxfep->mxfe_bmsr = MII_BMSR_ANA | MII_BMSR_100FDX | MII_BMSR_100HDX |
	    MII_BMSR_10FDX | MII_BMSR_10HDX;

	/* 100-T4 not supported with NWay */
	mxfep->mxfe_adv_100T4 = 0;

	/* make sure at least one valid mode is selected */
	if ((!mxfep->mxfe_adv_100fdx) &&
	    (!mxfep->mxfe_adv_100hdx) &&
	    (!mxfep->mxfe_adv_10fdx) &&
	    (!mxfep->mxfe_adv_10hdx)) {
		mxfe_error(mxfep->mxfe_dip, "No valid link mode selected.");
		mxfe_error(mxfep->mxfe_dip,
		    "Falling back to 10 Mbps Half-Duplex mode.");
		mxfep->mxfe_adv_10hdx = 1;
	}

	if (mxfep->mxfe_adv_aneg == 0) {
		/* forced mode */
		unsigned	restart;
		unsigned	nar;
		unsigned	tctl;
		nar = GETCSR(mxfep, MXFE_CSR_NAR);
		tctl = GETCSR(mxfep, MXFE_CSR_TCTL);

		restart = nar & (MXFE_TX_ENABLE | MXFE_RX_ENABLE);
		nar &= ~(MXFE_TX_ENABLE | MXFE_RX_ENABLE);

		nar &= ~(MXFE_NAR_FDX | MXFE_NAR_PORTSEL | MXFE_NAR_SCR |
		    MXFE_NAR_SPEED);
		tctl &= ~MXFE_TCTL_ANE;
		if (mxfep->mxfe_adv_100fdx) {
			nar |= MXFE_NAR_PORTSEL | MXFE_NAR_PCS | MXFE_NAR_SCR;
			nar |= MXFE_NAR_FDX;
		} else if (mxfep->mxfe_adv_100hdx) {
			nar |= MXFE_NAR_PORTSEL | MXFE_NAR_PCS | MXFE_NAR_SCR;
		} else if (mxfep->mxfe_adv_10fdx) {
			nar |= MXFE_NAR_FDX | MXFE_NAR_SPEED;
		} else { /* mxfep->mxfe_adv_10hdx */
			nar |= MXFE_NAR_SPEED;
		}

		PUTCSR(mxfep, MXFE_CSR_NAR, nar);
		PUTCSR(mxfep, MXFE_CSR_TCTL, tctl);
		if (restart) {
			nar |= restart;
			PUTCSR(mxfep, MXFE_CSR_NAR, nar);
		}
		/* Macronix initializations from Bolo Tsai */
		PUTCSR(mxfep, MXFE_CSR_MXMAGIC, 0x0b2c0000);
		PUTCSR(mxfep, MXFE_CSR_ACOMP, 0x11000);
	} else {
		mxfe_startnway(mxfep);
	}
	PUTCSR(mxfep, MXFE_CSR_TIMER, MXFE_TIMER_LOOP |
	    (MXFE_LINKTIMER * 1000 / MXFE_TIMER_USEC));
}

/*
 * MII management.
 */
static void
mxfe_phyinitmii(mxfe_t *mxfep)
{
	unsigned	phyaddr;
	unsigned	bmcr;
	unsigned	bmsr;
	unsigned	anar;
	unsigned	phyidr1;
	unsigned	phyidr2;
	int		retries;
	int		force;
	int		cnt;
	int		validmode;

	mxfep->mxfe_phyaddr = -1;

	/* search for first PHY we can find */
	for (phyaddr = 0; phyaddr < 32; phyaddr++) {
		bmcr = mxfe_miiread(mxfep, phyaddr, MII_REG_BMSR);
		if ((bmcr != 0) && (bmcr != 0xffff)) {
			mxfep->mxfe_phyaddr = phyaddr;
		}
	}

	phyidr1 = mxfe_miiread(mxfep, phyaddr, MII_REG_PHYIDR1);
	phyidr2 = mxfe_miiread(mxfep, phyaddr, MII_REG_PHYIDR2);

	DBG(MXFE_DPHY, "phy at %d: %x,%x", phyaddr, phyidr1, phyidr2);
	DBG(MXFE_DPHY, "bmsr = %x", mxfe_miiread(mxfep,
	    mxfep->mxfe_phyaddr, MII_REG_BMSR));
	DBG(MXFE_DPHY, "anar = %x", mxfe_miiread(mxfep,
	    mxfep->mxfe_phyaddr, MII_REG_ANAR));
	DBG(MXFE_DPHY, "anlpar = %x", mxfe_miiread(mxfep,
	    mxfep->mxfe_phyaddr, MII_REG_ANLPAR));
	DBG(MXFE_DPHY, "aner = %x", mxfe_miiread(mxfep,
	    mxfep->mxfe_phyaddr, MII_REG_ANER));

	DBG(MXFE_DPHY, "resetting phy");

	/* we reset the phy block */
	mxfe_miiwrite(mxfep, phyaddr, MII_REG_BMCR, MII_BMCR_RESET);
	/*
	 * wait for it to complete -- 500usec is still to short to
	 * bother getting the system clock involved.
	 */
	drv_usecwait(500);
	for (retries = 0; retries < 10; retries++) {
		if (mxfe_miiread(mxfep, phyaddr, MII_REG_BMCR) &
		    MII_BMCR_RESET) {
			drv_usecwait(500);
			continue;
		}
		break;
	}
	if (retries == 100) {
		mxfe_error(mxfep->mxfe_dip, "timeout waiting on phy to reset");
		return;
	}

	DBG(MXFE_DPHY, "phy reset complete");

	bmsr = mxfe_miiread(mxfep, phyaddr, MII_REG_BMSR);
	bmcr = mxfe_miiread(mxfep, phyaddr, MII_REG_BMCR);
	anar = mxfe_miiread(mxfep, phyaddr, MII_REG_ANAR);

	anar &= ~(MII_ANEG_100BT4 | MII_ANEG_100FDX | MII_ANEG_100HDX |
	    MII_ANEG_10FDX | MII_ANEG_10HDX);

	force = 0;
	validmode = 0;

	/* disable modes not supported in hardware */
	if (!(bmsr & MII_BMSR_100BT4)) {
		mxfep->mxfe_adv_100T4 = 0;
	} else {
		validmode = MII_ANEG_100BT4;
	}
	if (!(bmsr & MII_BMSR_100FDX)) {
		mxfep->mxfe_adv_100fdx = 0;
	} else {
		validmode = MII_ANEG_100FDX;
	}
	if (!(bmsr & MII_BMSR_100HDX)) {
		mxfep->mxfe_adv_100hdx = 0;
	} else {
		validmode = MII_ANEG_100HDX;
	}
	if (!(bmsr & MII_BMSR_10FDX)) {
		mxfep->mxfe_adv_10fdx = 0;
	} else {
		validmode = MII_ANEG_10FDX;
	}
	if (!(bmsr & MII_BMSR_10HDX)) {
		mxfep->mxfe_adv_10hdx = 0;
	} else {
		validmode = MII_ANEG_10HDX;
	}
	if (!(bmsr & MII_BMSR_ANA)) {
		mxfep->mxfe_adv_aneg = 0;
		force = 1;
	}

	cnt = 0;
        if (mxfep->mxfe_adv_100T4) {
                anar |= MII_ANEG_100BT4;
                cnt++;
        }
        if (mxfep->mxfe_adv_100fdx) {
                anar |= MII_ANEG_100FDX;
                cnt++;
        }
        if (mxfep->mxfe_adv_100hdx) {
                anar |= MII_ANEG_100HDX;
                cnt++;
        }
        if (mxfep->mxfe_adv_10fdx) {
                anar |= MII_ANEG_10FDX;
                cnt++;
        }
        if (mxfep->mxfe_adv_10hdx) {
                anar |= MII_ANEG_10HDX;
                cnt++;
        }

	/*
	 * Make certain at least one valid link mode is selected.
	 */
	if (!cnt) {
		char	*s;
		mxfe_error(mxfep->mxfe_dip, "No valid link mode selected.");
		switch (validmode) {
		case MII_ANEG_100BT4:
			s = "100 Base T4";
			mxfep->mxfe_adv_100T4 = 1;
			break;
		case MII_ANEG_100FDX:
			s = "100 Mbps Full-Duplex";
			mxfep->mxfe_adv_100fdx = 1;
			break;
		case MII_ANEG_100HDX:
			s = "100 Mbps Half-Duplex";
			mxfep->mxfe_adv_100hdx = 1;
			break;
		case MII_ANEG_10FDX:
			s = "10 Mbps Full-Duplex";
			mxfep->mxfe_adv_10fdx = 1;
			break;
		case MII_ANEG_10HDX:
			s = "10 Mbps Half-Duplex";
			mxfep->mxfe_adv_10hdx = 1;
			break;
		default:
			s = "unknown";
			break;
		}
		anar |= validmode;
		mxfe_error(mxfep->mxfe_dip, "Falling back to %s mode.", s);
	}

	if ((mxfep->mxfe_adv_aneg) && (bmsr & MII_BMSR_ANA)) {
		DBG(MXFE_DPHY, "using autoneg mode");
		bmcr = (MII_BMCR_ANEG | MII_BMCR_RANEG);
	} else {
		DBG(MXFE_DPHY, "using forced mode");
		force = 1;
		if (mxfep->mxfe_adv_100fdx) {
			bmcr = (MII_BMCR_SPEED | MII_BMCR_DUPLEX);
		} else if (mxfep->mxfe_adv_100hdx) {
			bmcr = MII_BMCR_SPEED;
		} else if (mxfep->mxfe_adv_10fdx) {
			bmcr = MII_BMCR_DUPLEX;
		} else {
			/* 10HDX */
			bmcr = 0;
		}
	}

	mxfep->mxfe_forcephy = 0;

	DBG(MXFE_DPHY, "programming anar to 0x%x", anar);
	mxfe_miiwrite(mxfep, phyaddr, MII_REG_ANAR, anar);
	DBG(MXFE_DPHY, "programming bmcr to 0x%x", bmcr);
	mxfe_miiwrite(mxfep, phyaddr, MII_REG_BMCR, bmcr);

	/*
	 * schedule a query of the link status
	 */
	PUTCSR(mxfep, MXFE_CSR_TIMER, MXFE_TIMER_LOOP |
	    (MXFE_LINKTIMER * 1000 / MXFE_TIMER_USEC));
}

static void
mxfe_reportlink(mxfe_t *mxfep)
{
	int changed = 0;

	if (mxfep->mxfe_ifspeed != mxfep->mxfe_lastifspeed) {
		mxfep->mxfe_lastifspeed = mxfep->mxfe_ifspeed;
		changed++;
	}
	if (mxfep->mxfe_duplex != mxfep->mxfe_lastduplex) {
		mxfep->mxfe_lastduplex = mxfep->mxfe_duplex;
		changed++;
	}
	if (mxfep->mxfe_linkup && changed) {
		char *media;
		switch (mxfep->mxfe_media) {
		case GLDM_TP:
			media = "Twisted Pair";
			break;
		case GLDM_PHYMII:
			media = "MII";
			break;
		case GLDM_FIBER:
			media = "Fiber";
			break;
		default:
			media = "Unknown";
			break;
		}
		cmn_err(CE_NOTE, mxfep->mxfe_ifspeed ?
		    "%s%d: %s %d Mbps %s-Duplex (%s) Link Up" :
		    "%s%d: Unknown %s MII Link Up",
		    ddi_driver_name(mxfep->mxfe_dip),
		    ddi_get_instance(mxfep->mxfe_dip),
		    mxfep->mxfe_forcephy ? "Forced" : "Auto-Negotiated",
		    (int)(mxfep->mxfe_ifspeed / 1000000),
		    mxfep->mxfe_duplex == GLD_DUPLEX_FULL ? "Full" : "Half",
		    media);
		mxfep->mxfe_lastlinkdown = 0;
	} else if (mxfep->mxfe_linkup) {
		mxfep->mxfe_lastlinkdown = 0;
	} else {
		/* link lost, warn once every 10 seconds */
		if ((ddi_get_time() - mxfep->mxfe_lastlinkdown) > 10) {
			/* we've lost link, only warn on transition */
			mxfe_error(mxfep->mxfe_dip,
			    "Link Down -- Cable Problem?");
			mxfep->mxfe_lastlinkdown = ddi_get_time();
		}
	}
}

static void
mxfe_checklink(mxfe_t *mxfep)
{
	switch (MXFE_MODEL(mxfep)) {
	case MXFE_MODEL_98713A:
		mxfe_checklinkmii(mxfep);
		break;
	default:
		mxfe_checklinknway(mxfep);
	}
}

static void
mxfe_checklinkmii(mxfe_t *mxfep)
{
	/* read MII state registers */
	ushort 		bmsr;
	ushort 		bmcr;
	ushort 		anar;
	ushort 		anlpar;
	ushort 		aner;

	mxfep->mxfe_media = GLDM_PHYMII;

	/* read this twice, to clear latched link state */
	bmsr = mxfe_miiread(mxfep, mxfep->mxfe_phyaddr, MII_REG_BMSR);
	bmsr = mxfe_miiread(mxfep, mxfep->mxfe_phyaddr, MII_REG_BMSR);
	bmcr = mxfe_miiread(mxfep, mxfep->mxfe_phyaddr, MII_REG_BMCR);
	anar = mxfe_miiread(mxfep, mxfep->mxfe_phyaddr, MII_REG_ANAR);
	anlpar = mxfe_miiread(mxfep, mxfep->mxfe_phyaddr, MII_REG_ANLPAR);
	aner = mxfe_miiread(mxfep, mxfep->mxfe_phyaddr, MII_REG_ANER);

	mxfep->mxfe_bmsr = bmsr;
	mxfep->mxfe_anlpar = anlpar;
	mxfep->mxfe_aner = aner;

	if (bmsr & MII_BMSR_RFAULT) {
		mxfe_error(mxfep->mxfe_dip, "Remote fault detected.");
	}
	if (bmsr & MII_BMSR_JABBER) {
		mxfe_error(mxfep->mxfe_dip, "Jabber condition detected.");
	}
	if ((bmsr & MII_BMSR_LINK) == 0) {
		/* no link */
		mxfep->mxfe_ifspeed = 0;
		mxfep->mxfe_duplex = GLD_DUPLEX_UNKNOWN;
		mxfep->mxfe_linkup = 0;
		mxfe_reportlink(mxfep);
		return;
	}

	DBG(MXFE_DCHATTY, "link up!");
	mxfep->mxfe_lastlinkdown = 0;
	mxfep->mxfe_linkup = 1;

	if (!(bmcr & MII_BMCR_ANEG)) {
		/* forced mode */
		if (bmcr & MII_BMCR_SPEED) {
			mxfep->mxfe_ifspeed = 100000000;
		} else {
			mxfep->mxfe_ifspeed = 10000000;
		}
		if (bmcr & MII_BMCR_DUPLEX) {
			mxfep->mxfe_duplex = GLD_DUPLEX_FULL;
		} else {
			mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
		}
	} else if ((!(bmsr & MII_BMSR_ANA)) || (!(bmsr & MII_BMSR_ANC))) {
		mxfep->mxfe_ifspeed = 0;
		mxfep->mxfe_duplex = GLD_DUPLEX_UNKNOWN;
	} else if (anar & anlpar & MII_ANEG_100BT4) {
		mxfep->mxfe_ifspeed = 100000000;
		mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
	} else if (anar & anlpar & MII_ANEG_100FDX) {
		mxfep->mxfe_ifspeed = 100000000;
		mxfep->mxfe_duplex = GLD_DUPLEX_FULL;
	} else if (anar & anlpar & MII_ANEG_100HDX) {
		mxfep->mxfe_ifspeed = 100000000;
		mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
	} else if (anar & anlpar & MII_ANEG_10FDX) {
		mxfep->mxfe_ifspeed = 10000000;
		mxfep->mxfe_duplex = GLD_DUPLEX_FULL;
	} else if (anar & anlpar & MII_ANEG_10HDX) {
		mxfep->mxfe_ifspeed = 10000000;
		mxfep->mxfe_duplex = GLD_DUPLEX_HALF;
	} else {
		mxfep->mxfe_ifspeed = 0;
		mxfep->mxfe_duplex = GLD_DUPLEX_UNKNOWN;
	}

	mxfe_reportlink(mxfep);
}

static void
mxfe_miitristate(mxfe_t *mxfep)
{
	unsigned val = MXFE_SROM_WRITE | MXFE_MII_CONTROL;
	PUTCSR(mxfep, MXFE_CSR_SPR, val);
	drv_usecwait(1);
	PUTCSR(mxfep, MXFE_CSR_SPR, val | MXFE_MII_CLOCK);
	drv_usecwait(1);
}

static void
mxfe_miiwritebit(mxfe_t *mxfep, int bit)
{
	unsigned val = bit ? MXFE_MII_DOUT : 0;
	PUTCSR(mxfep, MXFE_CSR_SPR, val);
	drv_usecwait(1);
	PUTCSR(mxfep, MXFE_CSR_SPR, val | MXFE_MII_CLOCK);
	drv_usecwait(1);
}

static int
mxfe_miireadbit(mxfe_t *mxfep)
{
	unsigned val = MXFE_MII_CONTROL | MXFE_SROM_READ;
	int bit;
	PUTCSR(mxfep, MXFE_CSR_SPR, val);
	drv_usecwait(1);
	bit = (GETCSR(mxfep, MXFE_CSR_SPR) & MXFE_MII_DIN) ? 1 : 0;
	PUTCSR(mxfep, MXFE_CSR_SPR, val | MXFE_MII_CLOCK);
	drv_usecwait(1);
	return (bit);
}

static unsigned
mxfe_miiread(mxfe_t *mxfep, int phy, int reg)
{
	switch (MXFE_MODEL(mxfep)) {
	case MXFE_MODEL_98713A:
		return (mxfe_miiread98713(mxfep, phy, reg));
	default:
		return (0xffff);
	}
}

static unsigned
mxfe_miireadgeneral(mxfe_t *mxfep, int phy, int reg)
{
	unsigned	value = 0;
	int		i;

	/* send the 32 bit preamble */
	for (i = 0; i < 32; i++) {
		mxfe_miiwritebit(mxfep, 1);
	}

	/* send the start code - 01b */
	mxfe_miiwritebit(mxfep, 0);
	mxfe_miiwritebit(mxfep, 1);

	/* send the opcode for read, - 10b */
	mxfe_miiwritebit(mxfep, 1);
	mxfe_miiwritebit(mxfep, 0);

	/* next we send the 5 bit phy address */
	for (i = 0x10; i > 0; i >>= 1) {
		mxfe_miiwritebit(mxfep, (phy & i) ? 1 : 0);
	}

	/* the 5 bit register address goes next */
	for (i = 0x10; i > 0; i >>= 1) {
		mxfe_miiwritebit(mxfep, (reg & i) ? 1 : 0);
	}

	/* turnaround - tristate followed by logic 0 */
	mxfe_miitristate(mxfep);
	mxfe_miiwritebit(mxfep, 0);

	/* read the 16 bit register value */
	for (i = 0x8000; i > 0; i >>= 1) {
		value <<= 1;
		value |= mxfe_miireadbit(mxfep);
	}
	mxfe_miitristate(mxfep);
	return (value);
}

static unsigned
mxfe_miiread98713(mxfe_t *mxfep, int phy, int reg)
{
	unsigned nar;
	unsigned retval;
	/*
	 * like an ordinary MII, but we have to turn off portsel while
	 * we read it.
	 */
	nar = GETCSR(mxfep, MXFE_CSR_NAR);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar & ~MXFE_NAR_PORTSEL);
	retval = mxfe_miireadgeneral(mxfep, phy, reg);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar);
	return (retval);
}

static void
mxfe_miiwrite(mxfe_t *mxfep, int phy, int reg, ushort val)
{
	switch (MXFE_MODEL(mxfep)) {
	case MXFE_MODEL_98713A:
		mxfe_miiwrite98713(mxfep, phy, reg, val);
		break;
	default:
		break;
	}
}

static void
mxfe_miiwritegeneral(mxfe_t *mxfep, int phy, int reg, ushort val)
{
	int i;

	/* send the 32 bit preamble */
	for (i = 0; i < 32; i++) {
		mxfe_miiwritebit(mxfep, 1);
	}

	/* send the start code - 01b */
	mxfe_miiwritebit(mxfep, 0);
	mxfe_miiwritebit(mxfep, 1);

	/* send the opcode for write, - 01b */
	mxfe_miiwritebit(mxfep, 0);
	mxfe_miiwritebit(mxfep, 1);

	/* next we send the 5 bit phy address */
	for (i = 0x10; i > 0; i >>= 1) {
		mxfe_miiwritebit(mxfep, (phy & i) ? 1 : 0);
	}

	/* the 5 bit register address goes next */
	for (i = 0x10; i > 0; i >>= 1) {
		mxfe_miiwritebit(mxfep, (reg & i) ? 1 : 0);
	}

	/* turnaround - tristate followed by logic 0 */
	mxfe_miitristate(mxfep);
	mxfe_miiwritebit(mxfep, 0);

	/* now write out our data (16 bits) */
	for (i = 0x8000; i > 0; i >>= 1) {
		mxfe_miiwritebit(mxfep, (val & i) ? 1 : 0);
	}

	/* idle mode */
	mxfe_miitristate(mxfep);
}

static void
mxfe_miiwrite98713(mxfe_t *mxfep, int phy, int reg, ushort val)
{
	unsigned nar;
	/*
	 * like an ordinary MII, but we have to turn off portsel while
	 * we read it.
	 */
	nar = GETCSR(mxfep, MXFE_CSR_NAR);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar & ~MXFE_NAR_PORTSEL);
	mxfe_miiwritegeneral(mxfep, phy, reg, val);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar);
}

static void
mxfe_setrxfilt(mxfe_t *mxfep)
{
	uint32_t		reenable;
	uint32_t		nar;
	uint32_t		ier;
	uint32_t		status;
	mxfe_desc_t		*tmdp;
	int
			i;
	if (mxfep->mxfe_flags & MXFE_SUSPENDED) {
		/* don't touch a suspended interface */
		return;
	}

	DBG(MXFE_DMACID, "preparing setup frame");
	/* stop both transmitter and receiver */
	nar = GETCSR(mxfep, MXFE_CSR_NAR);
	reenable = nar & (MXFE_TX_ENABLE | MXFE_RX_ENABLE);
	nar &= ~reenable;
	DBG(MXFE_DMACID, "setting NAR to %x", nar);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar);

	switch (mxfep->mxfe_promisc) {
	case GLD_MAC_PROMISC_PHYS:
		DBG(MXFE_DMACID, "setting promiscuous");
		CLRBIT(mxfep, MXFE_CSR_NAR, MXFE_RX_MULTI);
		SETBIT(mxfep, MXFE_CSR_NAR, MXFE_RX_PROMISC);
		break;
	case GLD_MAC_PROMISC_MULTI:
		DBG(MXFE_DMACID, "setting allmulti");
		CLRBIT(mxfep, MXFE_CSR_NAR, MXFE_RX_PROMISC);
		SETBIT(mxfep, MXFE_CSR_NAR, MXFE_RX_MULTI);
		break;
	case GLD_MAC_PROMISC_NONE:
		CLRBIT(mxfep, MXFE_CSR_NAR, MXFE_RX_PROMISC | MXFE_RX_MULTI);
		break;
	}

	/* turn off tx interrupts */
	ier = GETCSR(mxfep, MXFE_CSR_IER);
	DBG(MXFE_DMACID, "setting IER to %x",
	    ier & ~(MXFE_INT_TXNOBUF|MXFE_INT_TXOK));
	PUTCSR(mxfep, MXFE_CSR_IER, (ier & ~(MXFE_INT_TXNOBUF|MXFE_INT_TXOK)));

	/*
	 * reorder the transmit packets so when we restart the first tx
	 * packet is in slot zero.
	 */ 
	mxfe_txreorder(mxfep);

	tmdp = mxfep->mxfe_setup_desc;

	PUTDESC(mxfep, tmdp->desc_control,
	    MXFE_TXCTL_FIRST | MXFE_TXCTL_LAST | MXFE_SETUP_LEN |
	    MXFE_TXCTL_SETUP | MXFE_TXCTL_HASHPERF | MXFE_TXCTL_ENDRING);
	PUTDESC(mxfep, tmdp->desc_buffer1, mxfep->mxfe_setup_bufpaddr);
	PUTDESC(mxfep, tmdp->desc_buffer2, 0);
	PUTDESC(mxfep, tmdp->desc_status, MXFE_TXSTAT_OWN);

	ddi_dma_sync(mxfep->mxfe_setup_descdmah, 0, 0, DDI_DMA_SYNC_FORDEV);
	ddi_dma_sync(mxfep->mxfe_setup_bufdmah, 0, 0, DDI_DMA_SYNC_FORDEV);

	PUTCSR(mxfep, MXFE_CSR_TDB, mxfep->mxfe_setup_descpaddr);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar | MXFE_TX_ENABLE);
	/* bang the chip */
	PUTCSR(mxfep, MXFE_CSR_TDR, 0xffffffffU);

	DBG(MXFE_DMACID, "sent setup frame (0x%p)", mxfep->mxfe_setup_buf);

	/*
	 * wait up to 100 msec for setup frame to get loaded -- it
	 * typically should happen much sooner.
	 */
	for (i = 0; i < 100000; i += 100) {
		ddi_dma_sync(mxfep->mxfe_setup_descdmah, 0, sizeof (unsigned),
		    DDI_DMA_SYNC_FORKERNEL);
		status = GETDESC(mxfep, tmdp->desc_status);
		if ((status & MXFE_TXSTAT_OWN) == 0) {
			break;
		}
		drv_usecwait(10);
	}

	DBG(status & MXFE_TXSTAT_OWN ? MXFE_DWARN : MXFE_DMACID,
	    "%s setup frame after %d usec",
	    status & MXFE_TXSTAT_OWN ? "timed out" : "processed", i);

	PUTCSR(mxfep, MXFE_CSR_NAR, nar);
	PUTCSR(mxfep, MXFE_CSR_TDB, mxfep->mxfe_desc_txpaddr);
	PUTCSR(mxfep, MXFE_CSR_IER, ier);
	PUTCSR(mxfep, MXFE_CSR_NAR, nar | reenable);
	if (reenable & MXFE_TX_ENABLE) {
		PUTCSR(mxfep, MXFE_CSR_TDR, 0xffffffffU);
	}
	if (reenable & MXFE_RX_ENABLE) {
		PUTCSR(mxfep, MXFE_CSR_RDR, 0xffffffffU);
	}
}

/*
 * Multicast support.
 */
/*
 * Calculates the CRC of the multicast address, the lower 6 bits of
 * which are used to set up the multicast filter.
 */
static unsigned
mxfe_etherhashle(uchar_t *addrp)
{
	unsigned	hash = 0xffffffffU;
	int		byte;
	int		bit;
	uchar_t		curr;
	static unsigned	poly = 0xedb88320;

	/* little endian version of hashing code */
	for (byte = 0; byte < ETHERADDRL; byte++) {
		curr = addrp[byte];
		for (bit = 0; bit < 8; bit++, curr >>= 1) {
			hash = (hash >> 1) ^ (((hash ^ curr) & 1) ? poly : 0);
		}
	}
	return (hash);
}

static int
mxfe_start(gld_mac_info_t *macinfo)
{
	mxfe_t	*mxfep = (mxfe_t *)macinfo->gldm_private;

	/* grab exclusive access to the card */
	mutex_enter(&mxfep->mxfe_intrlock);
	mutex_enter(&mxfep->mxfe_xmtlock);

	if (mxfe_startmac(mxfep) == GLD_SUCCESS) {
		mxfep->mxfe_flags |= MXFE_RUNNING;
	}

	mutex_exit(&mxfep->mxfe_xmtlock);
	mutex_exit(&mxfep->mxfe_intrlock);
	return (GLD_SUCCESS);
}

static int
mxfe_stop(gld_mac_info_t *macinfo)
{
	mxfe_t	*mxfep = (mxfe_t *)macinfo->gldm_private;

	/* exclusive access to the hardware! */
	mutex_enter(&mxfep->mxfe_intrlock);
	mutex_enter(&mxfep->mxfe_xmtlock);

	mxfe_stopmac(mxfep);

	mxfep->mxfe_flags &= ~MXFE_RUNNING;
	mutex_exit(&mxfep->mxfe_xmtlock);
	mutex_exit(&mxfep->mxfe_intrlock);
	return (GLD_SUCCESS);
}

static int
mxfe_startmac(mxfe_t *mxfep)
{
	int	i;

	/* FIXME: do the power management thing */

	/* verify exclusive access to the card */
	ASSERT(mutex_owned(&mxfep->mxfe_intrlock));;
	ASSERT(mutex_owned(&mxfep->mxfe_xmtlock));

	/* stop the card */
	CLRBIT(mxfep, MXFE_CSR_NAR, MXFE_TX_ENABLE | MXFE_RX_ENABLE);

	/* free any pending buffers */
	for (i = 0; i < mxfep->mxfe_txring; i++) {
		if (mxfep->mxfe_txbufs[i]) {
			mxfe_freebuf(mxfep->mxfe_txbufs[i]);
			mxfep->mxfe_txbufs[i] = NULL;
		}
	}
	for (i = 0; i < mxfep->mxfe_rxring; i++) {
		if (mxfep->mxfe_rxbufs[i]) {
			mxfe_freebuf(mxfep->mxfe_rxbufs[i]);
			mxfep->mxfe_rxbufs[i] = NULL;
		}
	}

	/* reset the descriptor ring pointers */
	mxfep->mxfe_rxcurrent = 0;
	mxfep->mxfe_txreclaim = 0;
	mxfep->mxfe_txsend = 0;
	mxfep->mxfe_txavail = mxfep->mxfe_txring;
	PUTCSR(mxfep, MXFE_CSR_TDB, mxfep->mxfe_desc_txpaddr);
	PUTCSR(mxfep, MXFE_CSR_RDB, mxfep->mxfe_desc_rxpaddr);

	/*
	 * We only do this if we are initiating a hard reset
	 * of the chip, typically at start of day.  When we're
	 * just setting promiscuous mode or somesuch, this becomes
	 * wasteful.
	 */
	DBG(MXFE_DCHATTY, "phy reset");

	mxfe_phyinit(mxfep);

	/* point hardware at the descriptor rings */
	PUTCSR(mxfep, MXFE_CSR_RDB, mxfep->mxfe_desc_rxpaddr);
	PUTCSR(mxfep, MXFE_CSR_TDB, mxfep->mxfe_desc_txpaddr);

	/* set up transmit descriptor ring */
	for (i = 0; i < mxfep->mxfe_txring; i++) {
		mxfe_desc_t	*tmdp = &mxfep->mxfe_txdescp[i];
		unsigned	control = 0;
		if (i + 1 == mxfep->mxfe_txring) {
			control |= MXFE_TXCTL_ENDRING;
		}
		PUTDESC(mxfep, tmdp->desc_status, 0);
		PUTDESC(mxfep, tmdp->desc_control, control);
		PUTDESC(mxfep, tmdp->desc_buffer1, 0);
		PUTDESC(mxfep, tmdp->desc_buffer2, 0);
		SYNCDESC(mxfep, tmdp, DDI_DMA_SYNC_FORDEV);
	}

	/* make the receive buffers available */
	for (i = 0; i < mxfep->mxfe_rxring; i++) {
		mxfe_buf_t	*bufp;
		mxfe_desc_t	*rmdp = &mxfep->mxfe_rxdescp[i];
		unsigned	control;

		if ((bufp = mxfe_getbuf(mxfep, 1)) == NULL) {
			/* this should never happen! */
			mxfe_error(mxfep->mxfe_dip, "out of buffers!");
			return (DDI_FAILURE);
		}
		mxfep->mxfe_rxbufs[i] = bufp;

		control = MXFE_BUFSZ & MXFE_RXCTL_BUFLEN1;
		if (i + 1 == mxfep->mxfe_rxring) {
			control |= MXFE_RXCTL_ENDRING;
		}
		PUTDESC(mxfep, rmdp->desc_buffer1, bufp->bp_paddr);
		PUTDESC(mxfep, rmdp->desc_buffer2, 0);
		PUTDESC(mxfep, rmdp->desc_control, control);
		PUTDESC(mxfep, rmdp->desc_status, MXFE_RXSTAT_OWN);

		/* sync the descriptor for the device */
		SYNCDESC(mxfep, rmdp, DDI_DMA_SYNC_FORDEV);
	}

	DBG(MXFE_DCHATTY, "descriptors setup");

	/* clear the lost packet counter (cleared on read) */
	(void) GETCSR(mxfep, MXFE_CSR_LPC);

	/* enable interrupts */
	mxfe_enableinterrupts(mxfep);

	/* start the card */
	SETBIT(mxfep, MXFE_CSR_NAR, MXFE_TX_ENABLE | MXFE_RX_ENABLE);

	return (GLD_SUCCESS);
}

static int
mxfe_stopmac(mxfe_t *mxfep)
{
	/* exclusive access to the hardware! */
	ASSERT(mutex_owned(&mxfep->mxfe_intrlock));
	ASSERT(mutex_owned(&mxfep->mxfe_xmtlock));

	/* FIXME: possibly wait for transmits to drain */

	/* stop the card */
	CLRBIT(mxfep, MXFE_CSR_NAR, MXFE_TX_ENABLE | MXFE_RX_ENABLE);

	/* stop the on-card timer */
	PUTCSR(mxfep, MXFE_CSR_TIMER, 0);

	/* disable interrupts */
	mxfe_disableinterrupts(mxfep);

	mxfep->mxfe_linkup = 0;
	mxfep->mxfe_ifspeed = 0;
	mxfep->mxfe_duplex = GLD_DUPLEX_UNKNOWN;

	return (GLD_SUCCESS);
}

/*
 * Allocate descriptors.
 */
static int
mxfe_allocrings(mxfe_t *mxfep)
{
	int			size;
	int			rval;
	int			i;
	size_t			real_len;
	ddi_dma_cookie_t	cookie;
	uint			ncookies;
	ddi_dma_handle_t	dmah;
	ddi_acc_handle_t	acch;

	mxfep->mxfe_desc_dmahandle = NULL;
	mxfep->mxfe_desc_acchandle = NULL;

	size = (mxfep->mxfe_rxring + mxfep->mxfe_txring) * sizeof (mxfe_desc_t);
	rval = ddi_dma_alloc_handle(mxfep->mxfe_dip, &mxfe_dma_attr,
	    DDI_DMA_SLEEP, 0, &dmah);
	if (rval != DDI_SUCCESS) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to allocate DMA handle for media descriptors");
		return (DDI_FAILURE);
	}

	rval = ddi_dma_mem_alloc(dmah, size, &mxfe_devattr,
	    DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0, &mxfep->mxfe_desc_kaddr,
	    &real_len,  &acch);
	if (rval != DDI_SUCCESS) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to allocate DMA memory for media descriptors");
		ddi_dma_free_handle(&dmah);
		return (DDI_FAILURE);
	}

	rval = ddi_dma_addr_bind_handle(dmah, NULL, mxfep->mxfe_desc_kaddr,
	    size, DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, 0, &cookie, &ncookies);
	if (rval != DDI_DMA_MAPPED) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to bind DMA handle for media descriptors");
		ddi_dma_mem_free(&acch);
		ddi_dma_free_handle(&dmah);
		return (DDI_FAILURE);
	}

	/* we take the 32-bit physical address out of the cookie */
	mxfep->mxfe_desc_rxpaddr = cookie.dmac_address;
	mxfep->mxfe_desc_txpaddr = cookie.dmac_address +
	    (sizeof (mxfe_desc_t) * mxfep->mxfe_rxring);

	DBG(MXFE_DDMA, "rx phys addr = 0x%x", mxfep->mxfe_desc_rxpaddr);
	DBG(MXFE_DDMA, "tx phys addr = 0x%x", mxfep->mxfe_desc_txpaddr);

	if (ncookies != 1) {
		mxfe_error(mxfep->mxfe_dip, "too many DMA cookies for media "
		    "descriptors");
		(void) ddi_dma_unbind_handle(mxfep->mxfe_desc_dmahandle);
		ddi_dma_mem_free(&acch);
		ddi_dma_free_handle(&dmah);
		return (DDI_FAILURE);
	}

	size = sizeof (mxfe_buf_t *);

	/* allocate buffer pointers (not the buffers themselves, yet) */
	mxfep->mxfe_buftab = kmem_zalloc(mxfep->mxfe_numbufs * size, KM_SLEEP);
	mxfep->mxfe_txbufs = kmem_zalloc(mxfep->mxfe_txring * size, KM_SLEEP);
	mxfep->mxfe_rxbufs = kmem_zalloc(mxfep->mxfe_rxring * size, KM_SLEEP);
	mxfep->mxfe_temp_bufs = kmem_zalloc(mxfep->mxfe_txring * size,
	    KM_SLEEP);

	/* save off the descriptor handles */
	mxfep->mxfe_desc_dmahandle = dmah;
	mxfep->mxfe_desc_acchandle = acch;

	/* now allocate the actual buffers */
	for (i = 0; i < mxfep->mxfe_numbufs; i++) {
		mxfe_buf_t		*bufp;

		dmah = NULL;
		acch = NULL;
		bufp = kmem_zalloc(sizeof (mxfe_buf_t), KM_SLEEP);

		if (ddi_dma_alloc_handle(mxfep->mxfe_dip, &mxfe_dma_attr,
		    DDI_DMA_SLEEP, NULL, &dmah) != DDI_SUCCESS) {
			kmem_free(bufp, sizeof (mxfe_buf_t));
			return (DDI_FAILURE);
		}
		if (ddi_dma_mem_alloc(dmah, MXFE_BUFSZ, &mxfe_bufattr,
		    DDI_DMA_STREAMING, DDI_DMA_SLEEP, NULL,
		    &bufp->bp_buf, &real_len, &acch) != DDI_SUCCESS) {
			ddi_dma_free_handle(&dmah);
			return (DDI_FAILURE);
		}
		if (ddi_dma_addr_bind_handle(dmah, NULL, bufp->bp_buf,
		    real_len, DDI_DMA_STREAMING | DDI_DMA_RDWR,
		    DDI_DMA_SLEEP, 0, &cookie, &ncookies) != DDI_DMA_MAPPED) {
			(void) ddi_dma_mem_free(&acch);
			ddi_dma_free_handle(&dmah);
			kmem_free(bufp, sizeof (mxfe_buf_t));
			return (DDI_FAILURE);
		}

		bufp->bp_mxfep = mxfep;
		bufp->bp_frtn.free_func =  mxfe_freebuf;
		bufp->bp_frtn.free_arg = (caddr_t)bufp;
		bufp->bp_dma_handle = dmah;
		bufp->bp_acc_handle = acch;
		bufp->bp_paddr = cookie.dmac_address;
		DBG(MXFE_DDMA, "buf #%d phys addr = 0x%x", i,
		    bufp->bp_paddr);

		/* stick it in the stack */
		mxfep->mxfe_buftab[i] = bufp;
	}

	/* set the top of the stack */
	mxfep->mxfe_topbuf = mxfep->mxfe_numbufs;

	/* descriptor pointers */
	mxfep->mxfe_rxdescp = (mxfe_desc_t *)mxfep->mxfe_desc_kaddr;
	mxfep->mxfe_txdescp = mxfep->mxfe_rxdescp + mxfep->mxfe_rxring;

	mxfep->mxfe_rxcurrent = 0;
	mxfep->mxfe_txreclaim = 0;
	mxfep->mxfe_txsend = 0;
	mxfep->mxfe_txavail = mxfep->mxfe_txring;
	return (DDI_SUCCESS);
}

static void
mxfe_freerings(mxfe_t *mxfep)
{
	int			i;
	mxfe_buf_t		*bufp;
	ddi_dma_handle_t	dmah;
	ddi_acc_handle_t	acch;

	for (i = 0; i < mxfep->mxfe_numbufs; i++) {
		bufp = mxfep->mxfe_buftab[i];
		if (bufp != NULL) {
			dmah = bufp->bp_dma_handle;
			acch = bufp->bp_acc_handle;

			if (ddi_dma_unbind_handle(dmah) == DDI_SUCCESS) {
				ddi_dma_mem_free(&acch);
				ddi_dma_free_handle(&dmah);
			} else {
				mxfe_error(mxfep->mxfe_dip,
				    "ddi_dma_unbind_handle failed!");
			}
			kmem_free(bufp, sizeof (mxfe_buf_t));
		}
	}

	DBG(MXFE_DCHATTY, "freeing buffer pools");
	if (mxfep->mxfe_buftab) {
		kmem_free(mxfep->mxfe_buftab,
		    mxfep->mxfe_numbufs * sizeof (mxfe_buf_t *));
		mxfep->mxfe_buftab = NULL;
	}
	if (mxfep->mxfe_temp_bufs) {
		kmem_free(mxfep->mxfe_temp_bufs,
		    mxfep->mxfe_txring * sizeof (mxfe_buf_t *));
		mxfep->mxfe_temp_bufs = NULL;
	}
	if (mxfep->mxfe_txbufs) {
		kmem_free(mxfep->mxfe_txbufs,
		    mxfep->mxfe_txring * sizeof (mxfe_buf_t *));
		mxfep->mxfe_txbufs = NULL;
	}
	if (mxfep->mxfe_rxbufs) {
		kmem_free(mxfep->mxfe_rxbufs,
		    mxfep->mxfe_rxring * sizeof (mxfe_buf_t *));
		mxfep->mxfe_rxbufs = NULL;
	}
}

/*
 * Allocate setup frame.
 */
static int
mxfe_allocsetup(mxfe_t *mxfep)
{
	size_t			size;
	unsigned		ncookies;
	ddi_dma_cookie_t	cookie;

	/* first prepare the descriptor */

	size = sizeof (mxfe_desc_t);

	if (ddi_dma_alloc_handle(mxfep->mxfe_dip,  &mxfe_dma_attr,
	    DDI_DMA_SLEEP, 0, &mxfep->mxfe_setup_descdmah) != DDI_SUCCESS) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to allocate DMA handle for setup descriptor");
		return (DDI_FAILURE);
	}
	if (ddi_dma_mem_alloc(mxfep->mxfe_setup_descdmah, size, &mxfe_devattr,
	    DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0,
	    (caddr_t *)&mxfep->mxfe_setup_desc, &size,
	    &mxfep->mxfe_setup_descacch) != DDI_SUCCESS) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to allocate DMA memory for setup descriptor");
		mxfe_freesetup(mxfep);
		return (DDI_FAILURE);
	}

	bzero((void *)mxfep->mxfe_setup_desc, size);

	if (ddi_dma_addr_bind_handle(mxfep->mxfe_setup_descdmah, NULL,
	    (caddr_t)mxfep->mxfe_setup_desc, size,
	    DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0,
	    &cookie, &ncookies) != DDI_DMA_MAPPED) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to bind DMA handle for setup descriptor");
		mxfe_freesetup(mxfep);
		return (DDI_FAILURE);
	}

	mxfep->mxfe_setup_descpaddr = cookie.dmac_address;

	/* then prepare the buffer itself */

	size = MXFE_SETUP_LEN;

	if (ddi_dma_alloc_handle(mxfep->mxfe_dip,  &mxfe_dma_attr,
	    DDI_DMA_SLEEP, 0, &mxfep->mxfe_setup_bufdmah) != DDI_SUCCESS) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to allocate DMA handle for setup buffer");
		return (DDI_FAILURE);
	}

	bzero((void *)mxfep->mxfe_setup_buf, size);

	if (ddi_dma_addr_bind_handle(mxfep->mxfe_setup_bufdmah, NULL,
	    mxfep->mxfe_setup_buf, size, DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, 0, &cookie, &ncookies) != DDI_DMA_MAPPED) {
		mxfe_error(mxfep->mxfe_dip,
		    "unable to bind DMA handle for setup buffer");
		mxfe_freesetup(mxfep);
		return (DDI_FAILURE);
	}

	mxfep->mxfe_setup_bufpaddr = cookie.dmac_address;

	return (DDI_SUCCESS);
}

/*
 * Free setup frame.
 */
static void
mxfe_freesetup(mxfe_t *mxfep)
{
	DBG(MXFE_DCHATTY, "free descriptor DMA resources");
	if (mxfep->mxfe_setup_bufpaddr) {
		(void) ddi_dma_unbind_handle(mxfep->mxfe_setup_bufdmah);
	}
	if (mxfep->mxfe_setup_bufdmah) {
		ddi_dma_free_handle(&mxfep->mxfe_setup_bufdmah);
	}

	if (mxfep->mxfe_setup_descpaddr) {
		(void) ddi_dma_unbind_handle(mxfep->mxfe_setup_descdmah);
	}
	if (mxfep->mxfe_setup_descacch) {
		ddi_dma_mem_free(&mxfep->mxfe_setup_descacch);
	}
	if (mxfep->mxfe_setup_descdmah) {
		ddi_dma_free_handle(&mxfep->mxfe_setup_descdmah);
	}
}

/*
 * Buffer management.
 */
static mxfe_buf_t *
mxfe_getbuf(mxfe_t *mxfep, int pri)
{
	int		top;
	mxfe_buf_t	*bufp;

	mutex_enter(&mxfep->mxfe_buflock);
	top = mxfep->mxfe_topbuf;
	if ((top == 0) || ((pri == 0) && (top < MXFE_RSVDBUFS))) {
		mutex_exit(&mxfep->mxfe_buflock);
		return (NULL);
	}
	top--;
	bufp = mxfep->mxfe_buftab[top];
	mxfep->mxfe_buftab[top] = NULL;
	mxfep->mxfe_topbuf = top;
	mutex_exit(&mxfep->mxfe_buflock);
	return (bufp);
}

static void
mxfe_freebuf(mxfe_buf_t *bufp)
{
	mxfe_t *mxfep = bufp->bp_mxfep;
	mutex_enter(&mxfep->mxfe_buflock);
	mxfep->mxfe_buftab[mxfep->mxfe_topbuf++] = bufp;
	bufp->bp_flags = 0;
	mutex_exit(&mxfep->mxfe_buflock);
}

/*
 * Interrupt service routine.
 */
static unsigned
mxfe_intr(gld_mac_info_t *macinfo)
{
	mxfe_t		*mxfep = (mxfe_t *)macinfo->gldm_private;
	unsigned	status = 0;
	dev_info_t	*dip = mxfep->mxfe_dip;
	int		reset = 0;
	int		linkcheck = 0;

	mxfep->mxfe_intr++;

	mutex_enter(&mxfep->mxfe_intrlock);

	if (mxfep->mxfe_flags & MXFE_SUSPENDED) {
		/* we cannot receive interrupts! */
		mutex_exit(&mxfep->mxfe_intrlock);
		return (DDI_INTR_UNCLAIMED);
	}

	/* check interrupt status bit, did we interrupt? */
	status = GETCSR(mxfep, MXFE_CSR_SR);
	DBG(MXFE_DINTR, "interrupted, status = %x", status);

	if (!(status & MXFE_INT_ALL)) {
		DBG(MXFE_DWARN, " not us? status = %x mask=%x all=%x", status,
		    GETCSR(mxfep, MXFE_CSR_IER), MXFE_INT_ALL);
		mutex_exit(&mxfep->mxfe_intrlock);
		return (DDI_INTR_UNCLAIMED);
	}
	/* ack the interrupt */
	PUTCSR(mxfep, MXFE_CSR_SR, status & MXFE_INT_ALL);

	if (!(mxfep->mxfe_flags & MXFE_RUNNING)) {
		/* not running, don't touch anything */
		DBG(MXFE_DWARN, "int while not running, status = %x", status);
		mutex_exit(&mxfep->mxfe_intrlock);
		return (DDI_INTR_CLAIMED);
	}

	if ((MXFE_MODEL(mxfep) != MXFE_MODEL_98713A) &&
		((status & MXFE_INT_ANEG) || (mxfep->mxfe_linkup &&
		    (status & (MXFE_INT_10LINK | MXFE_INT_100LINK))))) {
		/* rescan the link */
		DBG(MXFE_DINTR, "link change interrupt!");
		linkcheck++;
	}

	if (status & MXFE_INT_TXNOBUF) {
		/* transmit completed, reclaim descriptors */
		mutex_enter(&mxfep->mxfe_xmtlock);
		mxfe_reclaim(mxfep);
		mutex_exit(&mxfep->mxfe_xmtlock);
		gld_sched(macinfo);
	}

	if (status & (MXFE_INT_RXOK | MXFE_INT_RXNOBUF)) {
		for (;;) {
			unsigned	status;
			mxfe_desc_t	*rmd =
			    &mxfep->mxfe_rxdescp[mxfep->mxfe_rxcurrent];

			/* sync it before we look at it */
			SYNCDESC(mxfep, rmd, DDI_DMA_SYNC_FORCPU);

			status = GETDESC(mxfep, rmd->desc_status);
			if (status & MXFE_RXSTAT_OWN) {
				/* chip is still chewing on it */
				break;
			}

			DBG(MXFE_DCHATTY, "reading packet at %d "
			    "(status = 0x%x, length=%d)",
			    mxfep->mxfe_rxcurrent, status,
			    MXFE_RXLENGTH(status));

			mxfe_read(mxfep, mxfep->mxfe_rxcurrent);

			/* advance to next RMD */
			mxfep->mxfe_rxcurrent++;
			mxfep->mxfe_rxcurrent %= mxfep->mxfe_rxring;

			/* give it back to the hardware */
			PUTDESC(mxfep, rmd->desc_status, MXFE_RXSTAT_OWN);
			SYNCDESC(mxfep, rmd, DDI_DMA_SYNC_FORDEV);

			/* poll demand the receiver */
			PUTCSR(mxfep, MXFE_CSR_RDR, 1);
		}
		/* we rec'd a packet, if linkdown, verify */
		if (mxfep->mxfe_linkup == 0) {
			linkcheck++;
		}
		DBG(MXFE_DCHATTY, "done receiving packets");
	}

	if (status & MXFE_INT_RXNOBUF) {
		mxfep->mxfe_norcvbuf++;
		DBG(MXFE_DINTR, "rxnobuf interrupt!");
	}

	if (status & (MXFE_INT_RXNOBUF | MXFE_INT_RXIDLE)) {
		/* restart the receiver */
		SETBIT(mxfep, MXFE_CSR_NAR, MXFE_RX_ENABLE);
	}

	if (status & MXFE_INT_BUSERR) {
		reset = 1;
		switch (status & MXFE_BERR_TYPE) {
		case MXFE_BERR_PARITY:
			mxfe_error(dip, "PCI parity error detected");
			break;
		case MXFE_BERR_TARGET_ABORT:
			mxfe_error(dip, "PCI target abort detected");
			break;
		case MXFE_BERR_MASTER_ABORT:
			mxfe_error(dip, "PCI master abort detected");
			break;
		default:
			mxfe_error(dip, "Unknown PCI bus error");
			break;
		}
	}
	if (status & MXFE_INT_TXUNDERFLOW) {
		/* stats updated in mxfe_reclaim() */
		reset = 1;
	}
	if (status & MXFE_INT_TXJABBER) {
		/* stats updated in mxfe_reclaim() */
		reset = 1;
	}
	if (status & MXFE_INT_RXJABBER) {
		mxfep->mxfe_errrcv++;
		reset = 1;
	}

	/*
	 * Update the missed frame count.
	 */
	mxfep->mxfe_missed += (GETCSR(mxfep, MXFE_CSR_LPC) & MXFE_LPC_COUNT);

	if (linkcheck) {
		mutex_enter(&mxfep->mxfe_xmtlock);
		mxfe_checklink(mxfep);
		mutex_exit(&mxfep->mxfe_xmtlock);
	}

	mutex_exit(&mxfep->mxfe_intrlock);

	if (reset) {
		/* XXXX: FIXME */
		/* reset the chip in an attempt to fix things */
		mxfe_stop(macinfo);
		mxfe_start(macinfo);
	}
	return (DDI_INTR_CLAIMED);
}

static void
mxfe_enableinterrupts(mxfe_t *mxfep)
{
	unsigned mask = MXFE_INT_WANTED;

	/* the following are used in NWay mode */
	mask |= MXFE_INT_ANEG;

	/* only interrupt on loss of link after link up is achieved */
	if (mxfep->mxfe_linkup > 0) {
		mask |= MXFE_INT_10LINK;
		mask |= MXFE_INT_100LINK;
	}
	DBG(MXFE_DINTR, "setting int mask to 0x%x", mask);
	PUTCSR(mxfep, MXFE_CSR_IER, mask);
}

static void
mxfe_disableinterrupts(mxfe_t *mxfep)
{
	/* disable further interrupts */
	PUTCSR(mxfep, MXFE_CSR_IER, 0);

	/* clear any pending interrupts */
	PUTCSR(mxfep, MXFE_CSR_SR, MXFE_INT_ALL);
}

static int
mxfe_send(gld_mac_info_t *macinfo, mblk_t *mp)
{
	mxfe_t			*mxfep = (mxfe_t *)macinfo->gldm_private;
	int			len;
	mxfe_buf_t		*bufp;
	mxfe_desc_t		*tmdp;
	unsigned		control;

	/* if the interface is suspended, put it back on the queue */
	if ((mxfep->mxfe_flags & MXFE_RUNNING) == 0) {
		return (GLD_NORESOURCES);
	}
	len = mxfe_msgsize(mp);
	if (len > ETHERMAX) {
		mxfep->mxfe_errxmt++;
		DBG(MXFE_DWARN, "output packet too long!");
		return (GLD_BADARG);
	}

	/* grab a transmit buffer */
	if ((bufp = mxfe_getbuf(mxfep, 1)) == NULL) {
		SETBIT(mxfep, MXFE_CSR_IER, MXFE_INT_TXNOBUF);
		DBG(MXFE_DXMIT, "no buffer available");
		return (GLD_NORESOURCES);
	}

	bufp->bp_len = len;

	/* prepare the buffer */
	if (mp->b_cont == NULL) {
		/* single mblk, easy fastpath case */
		bcopy(mp->b_rptr, (char *)bufp->bp_buf, len);
	} else {
		/* chained mblks, use a for-loop */
		mblk_t	*bp = mp;
		char	*dest = (char *)bufp->bp_buf;
		while (bp != NULL) {
			int n = MBLKL(bp);
			bcopy(bp->b_rptr, dest, n);
			dest += n;
			bp = bp->b_cont;
		}
	}

	DBG(MXFE_DCHATTY, "syncing mp = 0x%p,  buf 0x%p, len %d", mp,
	    bufp->bp_buf, len);

	/* sync the buffer for the device */
	SYNCBUF(bufp, len, DDI_DMA_SYNC_FORDEV);

	/* acquire transmit lock */
	mutex_enter(&mxfep->mxfe_xmtlock);

	/* if tx buffers are running low, try to get some more */
	if (mxfep->mxfe_txavail < MXFE_RECLAIM) {
		mxfe_reclaim(mxfep);
	}

	if (mxfep->mxfe_txavail == 0) {
		/* no more tmds */
		SETBIT(mxfep, MXFE_CSR_IER, MXFE_INT_TXNOBUF);
		mutex_exit(&mxfep->mxfe_xmtlock);
		mxfe_freebuf(bufp);
		DBG(MXFE_DXMIT, "out of tmds");
		return (GLD_NORESOURCES);
	}

	freemsg(mp);
	mp = NULL;

	mxfep->mxfe_txavail--;
	tmdp = &mxfep->mxfe_txdescp[mxfep->mxfe_txsend];
	control = MXFE_TXCTL_FIRST | MXFE_TXCTL_LAST |
	    (MXFE_TXCTL_BUFLEN1 & len) | MXFE_TXCTL_INTCMPLTE;
	if (mxfep->mxfe_txsend + 1 == mxfep->mxfe_txring) {
		control |= MXFE_TXCTL_ENDRING;
	}
	PUTDESC(mxfep, tmdp->desc_control, control);
	PUTDESC(mxfep, tmdp->desc_buffer1, bufp->bp_paddr);
	PUTDESC(mxfep, tmdp->desc_buffer2, 0);
	PUTDESC(mxfep, tmdp->desc_status, MXFE_TXSTAT_OWN);

	mxfep->mxfe_txbufs[mxfep->mxfe_txsend] = bufp;

	/* sync the descriptor out to the device */
	SYNCDESC(mxfep, tmdp, DDI_DMA_SYNC_FORDEV);

	/* update the ring pointer */
	mxfep->mxfe_txsend++;
	mxfep->mxfe_txsend %= mxfep->mxfe_txring;

	/* wake up the chip */
	PUTCSR(mxfep, MXFE_CSR_TDR, 0xffffffffU);

	mutex_exit(&mxfep->mxfe_xmtlock);

	return (GLD_SUCCESS);
}

/*
 * This moves all tx descriptors so that the first descriptor in the
 * ring is the first packet to transmit.  The transmitter must be idle
 * when this is called.  This is performed when doing a setup frame,
 * to ensure that when we restart we are able to continue transmitting
 * packets without dropping any.
 */
static void
mxfe_txreorder(mxfe_t *mxfep)
{
	int		num;
	int		i, j;
	mxfe_desc_t	*tmdp;

	num = mxfep->mxfe_txring - mxfep->mxfe_txavail;
	j = mxfep->mxfe_txreclaim;
	for (i = 0; i < num; i++) {
		tmdp = &mxfep->mxfe_txdescp[j];
		mxfep->mxfe_temp_bufs[i] = mxfep->mxfe_txbufs[j];
		mxfep->mxfe_txbufs[j] = NULL;
		/* clear the ownership bit */
		PUTDESC(mxfep, tmdp->desc_status, 0),
		j++;
		j %= mxfep->mxfe_txring;
	}
	for (i = 0; i < num; i++) {
		mxfe_buf_t	*bufp = mxfep->mxfe_temp_bufs[i];
		unsigned	control;

		mxfep->mxfe_txbufs[i] = bufp;
		tmdp = &mxfep->mxfe_txdescp[i];
		control = MXFE_TXCTL_FIRST | MXFE_TXCTL_LAST |
		    (MXFE_TXCTL_BUFLEN1 & bufp->bp_len) | MXFE_TXCTL_INTCMPLTE;
		if (i + 1 == mxfep->mxfe_txring) {
			control |= MXFE_TXCTL_ENDRING;
		}
		PUTDESC(mxfep, tmdp->desc_control, control);
		PUTDESC(mxfep, tmdp->desc_buffer1, bufp->bp_paddr);
		PUTDESC(mxfep, tmdp->desc_buffer2, 0);
		PUTDESC(mxfep, tmdp->desc_status, MXFE_TXSTAT_OWN);
	}
	mxfep->mxfe_txsend = i;
	mxfep->mxfe_txreclaim = 0;
}

/*
 * Reclaim buffers that have completed transmission.
 */
static void
mxfe_reclaim(mxfe_t *mxfep)
{
	mxfe_desc_t	*tmdp;
	int		freed = 0;

	for (;;) {
		unsigned	status;
		mxfe_buf_t	*bufp;

		if (mxfep->mxfe_txavail == mxfep->mxfe_txring) {
			/*
			 * We've emptied the ring, so we don't need to
			 * know about it again until the ring fills up
			 * next time.  (This logic is required because
			 * we only want to have this interrupt enabled
			 * when we run out of space in the ring; this
			 * significantly reduces the number interrupts
			 * required to transmit.)
			 */
			CLRBIT(mxfep, MXFE_CSR_IER, MXFE_INT_TXNOBUF);
			break;
		}

		tmdp = &mxfep->mxfe_txdescp[mxfep->mxfe_txreclaim];
		bufp = mxfep->mxfe_txbufs[mxfep->mxfe_txreclaim];

		/* sync it before we read it */
		SYNCDESC(mxfep, tmdp, DDI_DMA_SYNC_FORCPU);

		status = GETDESC(mxfep, tmdp->desc_status);

		if (status & MXFE_TXSTAT_OWN) {
			/* chip is still working on it, we're done */
			break;
		}

		/* update statistics */
		if (status & MXFE_TXSTAT_TXERR) {
			mxfep->mxfe_errxmt++;
		}

		if (status & MXFE_TXSTAT_JABBER) {
			/* transmit jabber timeout */
			DBG(MXFE_DXMIT, "tx jabber!");
			mxfep->mxfe_macxmt_errors++;
		}
		if (status & (MXFE_TXSTAT_CARRLOST | MXFE_TXSTAT_NOCARR)) {
			DBG(MXFE_DXMIT, "no carrier!");
			mxfe_checklink(mxfep);
			mxfep->mxfe_carrier_errors++;
		}

		if (status & MXFE_TXSTAT_UFLOW) {
			DBG(MXFE_DXMIT, "tx underflow!");
			mxfep->mxfe_underflow++;
			mxfep->mxfe_macxmt_errors++;
		}

		/* only count SQE errors if the test is not disabled */
		if ((status & MXFE_TXSTAT_SQE) &&
		    ((GETCSR(mxfep, MXFE_CSR_NAR) & MXFE_NAR_HBD) == 0)) {
			mxfep->mxfe_sqe_errors++;
			mxfep->mxfe_macxmt_errors++;
		}

		if (status & MXFE_TXSTAT_DEFER) {
			mxfep->mxfe_defer_xmts++;
		}

		/* collision counting */
		if (status & MXFE_TXSTAT_LATECOL) {
			DBG(MXFE_DXMIT, "tx late collision!");
			mxfep->mxfe_tx_late_collisions++;
			mxfep->mxfe_collisions++;
		} else if (status & MXFE_TXSTAT_EXCOLL) {
			DBG(MXFE_DXMIT, "tx excessive collisions!");
			mxfep->mxfe_ex_collisions++;
			mxfep->mxfe_collisions += 16;
		} else if (MXFE_TXCOLLCNT(status) == 1) {
			mxfep->mxfe_collisions++;
			mxfep->mxfe_first_collisions++;
		} else if (MXFE_TXCOLLCNT(status)) {
			mxfep->mxfe_collisions += MXFE_TXCOLLCNT(status);
			mxfep->mxfe_multi_collisions += MXFE_TXCOLLCNT(status);
		}

		/* release the buffer */
		mxfe_freebuf(bufp);
		mxfep->mxfe_txbufs[mxfep->mxfe_txreclaim] = NULL;
		mxfep->mxfe_txavail++;
		mxfep->mxfe_txreclaim++;
		mxfep->mxfe_txreclaim %= mxfep->mxfe_txring;
		freed++;
	}
}

static void
mxfe_read(mxfe_t *mxfep, int index)
{
	unsigned		status;
	unsigned		length;
	mxfe_desc_t		*rmdp;
	mxfe_buf_t		*bufp;
	int			good = 1;
	mblk_t			*mp;

	rmdp = &mxfep->mxfe_rxdescp[index];
	bufp = mxfep->mxfe_rxbufs[index];
	status = rmdp->desc_status;
	length = MXFE_RXLENGTH(status);

	/* discard the ethernet frame checksum */
	length -= ETHERFCSL;

	if ((status & MXFE_RXSTAT_LAST) == 0) {
		/* its an oversize packet!  ignore it for now */
		DBG(MXFE_DRECV, "rx oversize packet");
		return;
	}

	if (status & MXFE_RXSTAT_DESCERR) {
		DBG(MXFE_DRECV, "rx descriptor error "
		    "(index = %d, length = %d)", index, length);
		mxfep->mxfe_macrcv_errors++;
		good = 0;
	}
	if (status & MXFE_RXSTAT_RUNT) {
		DBG(MXFE_DRECV, "runt frame (index = %d, length =%d)",
		    index, length);
		mxfep->mxfe_runt++;
		mxfep->mxfe_macrcv_errors++;
		good = 0;
	}
	if ((status & MXFE_RXSTAT_FIRST) == 0) {
		/*
		 * this should also be a toolong, but specifically we
		 * cannot send it up, because we don't have the whole
		 * frame.
		 */
		DBG(MXFE_DRECV, "rx fragmented, dropping it");
		good = 0;
	}
	if (status & (MXFE_RXSTAT_TOOLONG | MXFE_RXSTAT_WATCHDOG)) {
		DBG(MXFE_DRECV, "rx toolong or watchdog seen");
		mxfep->mxfe_toolong_errors++;
		good = 0;
	}
	if (status & MXFE_RXSTAT_COLLSEEN) {
		DBG(MXFE_DRECV, "rx late collision");
		/* this should really be rx_late_collisions */
		mxfep->mxfe_collisions++;
		good = 0;
	}
	if (status & MXFE_RXSTAT_DRIBBLE) {
		DBG(MXFE_DRECV, "rx dribbling");
		mxfep->mxfe_align_errors++;
		good = 0;
	}
	if (status & MXFE_RXSTAT_CRCERR) {
		DBG(MXFE_DRECV, "rx frame crc error");
		mxfep->mxfe_fcs_errors++;
		good = 0;
	}
	if (status & MXFE_RXSTAT_OFLOW) {
		DBG(MXFE_DRECV, "rx fifo overflow");
		mxfep->mxfe_overflow++;
		mxfep->mxfe_macrcv_errors++;
		good = 0;
	}

	if (length > ETHERMAX) {
		/* chip garbled length in descriptor field? */
		DBG(MXFE_DRECV, "packet length too big (%d)", length);
		mxfep->mxfe_toolong_errors++;
		good = 0;
	}

	/* last fragment in packet, do the bookkeeping */
	if (!good) {
		mxfep->mxfe_errrcv++;
		/* packet was munged, drop it */
		DBG(MXFE_DRECV, "dropping frame, status = 0x%x", status);
		return;
	}

	/* sync the buffer before we look at it */
	SYNCBUF(bufp, length, DDI_DMA_SYNC_FORCPU);

	/*
	 * FIXME: note, for efficiency we may wish to "loan-up"
	 * buffers, but for now we just use mblks and copy it.
	 */
	if ((mp = allocb(length + MXFE_HEADROOM, BPRI_LO)) == NULL) {
		mxfep->mxfe_norcvbuf++;
		return;
	}

	/* offset by headroom (should be 2 modulo 4), avoids bcopy in IP */
	mp->b_rptr += MXFE_HEADROOM;
	bcopy((char *)bufp->bp_buf, mp->b_rptr, length);
	mp->b_wptr = mp->b_rptr + length;

	gld_recv(mxfep->mxfe_macinfo, mp);
}

/*
 * Streams and DLPI utility routines.  (Duplicated in strsun.h and
 * sundlpi.h, but we cannot safely use those for DDI compatibility.)
 */

static int
mxfe_msgsize(mblk_t *mp)
{
	int n;
	for (n = 0; mp != NULL; mp = mp->b_cont) {
		n += MBLKL(mp);
	}
	return (n);
}

static void
mxfe_miocack(queue_t *wq, mblk_t *mp, uint8_t type, int count, int error)
{
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;

	/*
	 * For now we assume that there is exactly one reference
	 * to the mblk.  If this isn't true, then its an error.
	 */
	mp->b_datap->db_type = type;
	iocp->ioc_count = count;
	iocp->ioc_error = error;
	qreply(wq, mp);
}

static int
mxfe_get_stats(gld_mac_info_t *macinfo, struct gld_stats *sp)
{
	mxfe_t	*mxfep = (mxfe_t *)macinfo->gldm_private;
#if 0
	mutex_enter(&mxfep->mxfe_xmtlock);
	if (mxfep->mxfe_flags & MXFE_RUNNING) {
		/* reclaim tx bufs to pick up latest stats */
		mxfe_reclaim(mxfep);
	}
	mutex_exit(&mxfep->mxfe_xmtlock);

	/* update the missed frame count from CSR */
	if (mxfep->mxfe_flags & MXFE_RUNNING) {
		mxfep->mxfe_missed +=
			(GETCSR(mxfep, MXFE_CSR_LPC) & MXFE_LPC_COUNT);
	}
#endif
	sp->glds_speed = mxfep->mxfe_ifspeed;
	sp->glds_media = mxfep->mxfe_media;
	sp->glds_intr = mxfep->mxfe_intr;
	sp->glds_norcvbuf = mxfep->mxfe_norcvbuf;
	sp->glds_errrcv = mxfep->mxfe_errrcv;
	sp->glds_errxmt = mxfep->mxfe_errxmt;
	sp->glds_missed = mxfep->mxfe_missed;
	sp->glds_underflow = mxfep->mxfe_underflow;
	sp->glds_overflow = mxfep->mxfe_overflow;
	sp->glds_frame = mxfep->mxfe_align_errors;
	sp->glds_crc = mxfep->mxfe_fcs_errors;
	sp->glds_duplex = mxfep->mxfe_duplex;
	sp->glds_nocarrier = mxfep->mxfe_carrier_errors;
	sp->glds_collisions = mxfep->mxfe_collisions;
	sp->glds_excoll = mxfep->mxfe_ex_collisions;
	sp->glds_xmtlatecoll = mxfep->mxfe_tx_late_collisions;
	sp->glds_defer = mxfep->mxfe_defer_xmts;
	sp->glds_dot3_first_coll = mxfep->mxfe_first_collisions;
	sp->glds_dot3_multi_coll = mxfep->mxfe_multi_collisions;
	sp->glds_dot3_sqe_error = mxfep->mxfe_sqe_errors;
	sp->glds_dot3_mac_xmt_error = mxfep->mxfe_macxmt_errors;
	sp->glds_dot3_mac_rcv_error = mxfep->mxfe_macrcv_errors;
	sp->glds_dot3_frame_too_long = mxfep->mxfe_toolong_errors;
	sp->glds_short = mxfep->mxfe_runt;
	return (GLD_SUCCESS);
}

/*
 * NDD support.
 */
static mxfe_nd_t *
mxfe_ndfind(mxfe_t *mxfep, char *name)
{
	mxfe_nd_t	*ndp;

	for (ndp = mxfep->mxfe_ndp; ndp != NULL; ndp = ndp->nd_next) {
		if (!strcmp(name, ndp->nd_name)) {
			break;
		}
	}
	return (ndp);
}

static void
mxfe_ndadd(mxfe_t *mxfep, char *name, mxfe_nd_pf_t get, mxfe_nd_pf_t set,
    intptr_t arg1, intptr_t arg2)
{
	mxfe_nd_t	*newndp;
	mxfe_nd_t	**ndpp;

	newndp = (mxfe_nd_t *)kmem_alloc(sizeof (mxfe_nd_t), KM_SLEEP);
	newndp->nd_next = NULL;
	newndp->nd_name = name;
	newndp->nd_get = get;
	newndp->nd_set = set;
	newndp->nd_arg1 = arg1;
	newndp->nd_arg2 = arg2;

	/* seek to the end of the list */
	for (ndpp = &mxfep->mxfe_ndp; *ndpp; ndpp = &(*ndpp)->nd_next) {
	}

	*ndpp = newndp;
}

static void
mxfe_ndempty(mblk_t *mp)
{
	while (mp != NULL) {
		mp->b_rptr = mp->b_datap->db_base;
		mp->b_wptr = mp->b_rptr;
		/* bzero(mp->b_wptr, MBLKSIZE(mp));*/
		mp = mp->b_cont;
	}
}

static void
mxfe_ndget(mxfe_t *mxfep, queue_t *wq, mblk_t *mp)
{
	mblk_t		*nmp = mp->b_cont;
	mxfe_nd_t	*ndp;
	int		rv;
	char		name[128];

	/* assumption, name will fit in first mblk of chain */
	if ((nmp == NULL) || (MBLKSIZE(nmp) < 1)) {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		return;
	}

	if (mxfe_ndparselen(nmp) >= sizeof (name)) {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
	}
	mxfe_ndparsestring(nmp, name, sizeof (name));

	/* locate variable */
	if ((ndp = mxfe_ndfind(mxfep, name)) == NULL) {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		return;
	}

	/* locate set callback */
	if (ndp->nd_get == NULL) {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EACCES);
		return;
	}

	/* clear the result buffer */
	mxfe_ndempty(nmp);

	rv = (*ndp->nd_get)(mxfep, nmp, ndp);
	if (rv == 0) {
		/* add final null bytes */
		rv = mxfe_ndaddbytes(nmp, "\0", 1);
	}

	if (rv == 0) {
		mxfe_miocack(wq, mp, M_IOCACK, mxfe_msgsize(nmp), 0);
	} else {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, rv);
	}
}

static void
mxfe_ndset(mxfe_t *mxfep, queue_t *wq, mblk_t *mp)
{
	mblk_t		*nmp = mp->b_cont;
	mxfe_nd_t	*ndp;
	int		rv;
	char		name[128];

	/* assumption, name will fit in first mblk of chain */
	if ((nmp == NULL) || (MBLKSIZE(nmp) < 1)) {
		return;
	}

	if (mxfe_ndparselen(nmp) >= sizeof (name)) {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
	}
	mxfe_ndparsestring(nmp, name, sizeof (name));

	/* locate variable */
	if ((ndp = mxfe_ndfind(mxfep, name)) == NULL) {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		return;
	}

	/* locate set callback */
	if (ndp->nd_set == NULL) {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, EACCES);
		return;
	}

	rv = (*ndp->nd_set)(mxfep, nmp, ndp);

	if (rv == 0) {
		mxfe_miocack(wq, mp, M_IOCACK, 0, 0);
	} else {
		mxfe_miocack(wq, mp, M_IOCNAK, 0, rv);
	}
}

static int
mxfe_ndaddbytes(mblk_t *mp, char *bytes, int cnt)
{
	int index;
	for (index = 0; index < cnt; index++) {
		while (mp && (mp->b_wptr >= DB_LIM(mp))) {
			mp = mp->b_cont;
		}
		if (mp == NULL) {
			return (ENOSPC);
		}
		*(mp->b_wptr) = *bytes;
		mp->b_wptr++;
		bytes++;
	}
	return (0);
}

static int
mxfe_ndaddstr(mblk_t *mp, char *str, int addnull)
{
	/* store the string, plus the terminating null */
	return (mxfe_ndaddbytes(mp, str, strlen(str) + (addnull ? 1 : 0)));
}

static int
mxfe_ndparselen(mblk_t *mp)
{
	int	len = 0;
	int	done = 0;
	uchar_t	*ptr;

	while (mp && !done) {
		for (ptr = mp->b_rptr; ptr < mp->b_wptr; ptr++) {
			if (!(*ptr)) {
				done = 1;
				break;
			}
			len++;
		}
		mp = mp->b_cont;
	}
	return (len);
}

static int
mxfe_ndparseint(mblk_t *mp)
{
	int	done = 0;
	int	val = 0;
	while (mp && !done) {
		while (mp->b_rptr < mp->b_wptr) {
			uchar_t ch = *(mp->b_rptr);
			mp->b_rptr++;
			if ((ch >= '0') && (ch <= '9')) {
				val *= 10;
				val += ch - '0';
			} else if (ch == 0) {
				mxfe_t *mxfep = NULL;
				DBG(MXFE_DPHY, "parsed value %d", val);
				return (val);
			} else {
				/* parse error, put back rptr */
				mp->b_rptr--;
				return (val);
			}
		}
		mp = mp->b_cont;
	}
	return (val);
}

static void
mxfe_ndparsestring(mblk_t *mp, char *buf, int maxlen)
{
	int	done = 0;
	int	len = 0;

	/* ensure null termination */
	buf[maxlen - 1] = 0;
	while (mp && !done) {
		while (mp->b_rptr < mp->b_wptr) {
			char ch = *((char *)mp->b_rptr);
			mp->b_rptr++;
			buf[len++] = ch;
			if ((ch == 0) || (len == maxlen)) {
				return;
			}
		}
		mp = mp->b_cont;
	}
}

static int
mxfe_ndquestion(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	for (ndp = mxfep->mxfe_ndp; ndp; ndp = ndp->nd_next) {
		int	rv;
		char	*s;
		if ((rv = mxfe_ndaddstr(mp, ndp->nd_name, 0)) != 0) {
			return (rv);
		}
		if (ndp->nd_get && ndp->nd_set) {
			s = " (read and write)";
		} else if (ndp->nd_get) {
			s = " (read only)";
		} else if (ndp->nd_set) {
			s = " (write only)";
		} else {
			s = " (no read or write)";
		}
		if ((rv = mxfe_ndaddstr(mp, s, 1)) != 0) {
			return (rv);
		}
	}
	return (0);
}

static int
mxfe_ndgetlinkstatus(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	unsigned	val;
	mutex_enter(&mxfep->mxfe_xmtlock);
	val = mxfep->mxfe_linkup;
	mutex_exit(&mxfep->mxfe_xmtlock);

	return (mxfe_ndaddstr(mp, val ? "1" : "0", 1));
}

static int
mxfe_ndgetlinkspeed(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	unsigned	val;
	char		buf[16];
	mutex_enter(&mxfep->mxfe_xmtlock);
	val = mxfep->mxfe_ifspeed;
	mutex_exit(&mxfep->mxfe_xmtlock);
	/* convert from bps to Mbps */
	sprintf(buf, "%d", val / 1000000);
	return (mxfe_ndaddstr(mp, buf, 1));
}

static int
mxfe_ndgetlinkmode(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	unsigned	val;
	mutex_enter(&mxfep->mxfe_xmtlock);
	val = mxfep->mxfe_duplex;
	mutex_exit(&mxfep->mxfe_xmtlock);
	return (mxfe_ndaddstr(mp, val == GLD_DUPLEX_FULL ? "1" : "0", 1));
}

static int
mxfe_ndgetsrom(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	unsigned	val;
	int		i;
	char		buf[80];

	for (i = 0; i < (1 << mxfep->mxfe_sromwidth); i++) {
		val = mxfe_readsromword(mxfep, i);
		sprintf(buf, "%s%04x", i % 8 ? " " : "", val);
		mxfe_ndaddstr(mp, buf, ((i % 8) == 7) ? 1 : 0);
	}
	return (0);
}

static int
mxfe_ndgetstring(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	char	*s = (char *)ndp->nd_arg1;
	return (mxfe_ndaddstr(mp, s ? s : "", 1));
}

static int
mxfe_ndgetbit(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	unsigned        val;
	unsigned        mask;

	val = *(unsigned *)ndp->nd_arg1;
	mask = (unsigned)ndp->nd_arg2;

        return (mxfe_ndaddstr(mp, val & mask ? "1" : "0", 1));

}

static int
mxfe_ndgetadv(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	unsigned	val;

	mutex_enter(&mxfep->mxfe_xmtlock);
	val = *((unsigned *)ndp->nd_arg1);
	mutex_exit(&mxfep->mxfe_xmtlock);

	return (mxfe_ndaddstr(mp, val ? "1" : "0", 1));
}

static int
mxfe_ndsetadv(mxfe_t *mxfep, mblk_t *mp, mxfe_nd_t *ndp)
{
	unsigned	*ptr = (unsigned *)ndp->nd_arg1;

	mutex_enter(&mxfep->mxfe_xmtlock);

	*ptr = (mxfe_ndparseint(mp) ? 1 : 0);
	/* now reset the phy */
	if ((mxfep->mxfe_flags & MXFE_SUSPENDED) == 0) {
		mxfe_phyinit(mxfep);
	}
	mutex_exit(&mxfep->mxfe_xmtlock);

	return (0);
}

static void
mxfe_ndfini(mxfe_t *mxfep)
{
	mxfe_nd_t	*ndp;

	while ((ndp = mxfep->mxfe_ndp) != NULL) {
		mxfep->mxfe_ndp = ndp->nd_next;
		kmem_free(ndp, sizeof (mxfe_nd_t));
	}
}

static void
mxfe_ndinit(mxfe_t *mxfep)
{
	mxfe_ndadd(mxfep, "?", mxfe_ndquestion, NULL, 0, 0);
	mxfe_ndadd(mxfep, "model", mxfe_ndgetstring, NULL,
	    (intptr_t)mxfep->mxfe_cardp->card_cardname, 0);
	mxfe_ndadd(mxfep, "link_status", mxfe_ndgetlinkstatus, NULL, 0, 0);
	mxfe_ndadd(mxfep, "link_speed", mxfe_ndgetlinkspeed, NULL, 0, 0);
	mxfe_ndadd(mxfep, "link_mode", mxfe_ndgetlinkmode, NULL, 0, 0);
	mxfe_ndadd(mxfep, "driver_version", mxfe_ndgetstring, NULL,
	    (intptr_t)mxfe_version, 0);
	mxfe_ndadd(mxfep, "adv_autoneg_cap", mxfe_ndgetadv, mxfe_ndsetadv,
	    (intptr_t)mxfep->mxfe_adv_aneg, 0);
	mxfe_ndadd(mxfep, "adv_100T4_cap", mxfe_ndgetadv, mxfe_ndsetadv,
	    (intptr_t)mxfep->mxfe_adv_100T4, 0);
	mxfe_ndadd(mxfep, "adv_100fdx_cap", mxfe_ndgetadv, mxfe_ndsetadv,
	    (intptr_t)mxfep->mxfe_adv_100fdx, 0);
	mxfe_ndadd(mxfep, "adv_100hdx_cap", mxfe_ndgetadv, mxfe_ndsetadv,
	    (intptr_t)mxfep->mxfe_adv_100hdx, 0);
	mxfe_ndadd(mxfep, "adv_10fdx_cap",  mxfe_ndgetadv, mxfe_ndsetadv,
	    (intptr_t)mxfep->mxfe_adv_10fdx, 0);
	mxfe_ndadd(mxfep, "adv_10hdx_cap",  mxfe_ndgetadv, mxfe_ndsetadv,
	    (intptr_t)mxfep->mxfe_adv_10hdx, 0);
	mxfe_ndadd(mxfep, "autoneg_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_bmsr, MII_BMSR_ANA);
	mxfe_ndadd(mxfep, "100T4_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_bmsr, MII_BMSR_100BT4);
	mxfe_ndadd(mxfep, "100fdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_bmsr, MII_BMSR_100FDX);
	mxfe_ndadd(mxfep, "100hdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_bmsr, MII_BMSR_100HDX);
	mxfe_ndadd(mxfep, "10fdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_bmsr, MII_BMSR_10FDX);
	mxfe_ndadd(mxfep, "10hdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_bmsr, MII_BMSR_10HDX);
	/* XXX: this needs ANER */
	mxfe_ndadd(mxfep, "lp_autoneg_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_aner, MII_ANER_LPANA);
	mxfe_ndadd(mxfep, "lp_100T4_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_anlpar, MII_ANEG_100BT4);
	mxfe_ndadd(mxfep, "lp_100fdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_anlpar, MII_ANEG_100FDX);
	mxfe_ndadd(mxfep, "lp_100hdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_anlpar, MII_ANEG_100HDX);
	mxfe_ndadd(mxfep, "lp_10fdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_anlpar, MII_ANEG_10HDX);
	mxfe_ndadd(mxfep, "lp_10hdx_cap", mxfe_ndgetbit, NULL,
	    (intptr_t)&mxfep->mxfe_anlpar, MII_ANEG_10HDX);
	mxfe_ndadd(mxfep, "srom", mxfe_ndgetsrom, NULL, 0, 0);
}

/*
 * Debugging and error reporting.
 */
static void
mxfe_verror(dev_info_t *dip, int level, char *fmt, va_list ap)
{
	char	buf[256];
	vsprintf(buf, fmt, ap);
	if (dip) {
		cmn_err(level, level == CE_CONT ? "%s%d: %s\n" : "%s%d: %s",
		    ddi_driver_name(dip), ddi_get_instance(dip), buf);
	} else {
		cmn_err(level, level == CE_CONT ? "%s: %s\n" : "%s: %s",
		    MXFE_IDNAME, buf);
	}
}

static void
mxfe_error(dev_info_t *dip, char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	mxfe_verror(dip, CE_WARN, fmt, ap);
	va_end(ap);
}

#ifdef DEBUG

static void
mxfe_dprintf(mxfe_t *mxfep, const char *func, int level, char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	if (mxfe_debug & level) {
		char	tag[64];
		char	buf[256];

		if (mxfep && mxfep->mxfe_dip) {
			sprintf(tag, "%s%d", ddi_driver_name(mxfep->mxfe_dip),
			    ddi_get_instance(mxfep->mxfe_dip));
		} else {
			sprintf(tag, "%s", MXFE_IDNAME);
		}

		sprintf(buf, "%s: %s: %s", tag, func, fmt);

		vcmn_err(CE_CONT, buf, ap);
	}
	va_end(ap);
}

#endif
