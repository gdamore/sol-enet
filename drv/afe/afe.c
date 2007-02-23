/*
 * Solaris DLPI driver for ethernet cards based on the ADMtek Centaur
 *
 * Copyright (c) 2001-2006 by Garrett D'Amore <garrett@damore.org>.
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

#ident	"@(#)$Id: afe.c,v 1.13 2007/02/23 02:56:30 gdamore Exp $"

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
#include <sys/afe.h>
#include <sys/afeimpl.h>

/*
 * Driver globals.
 */

/* patchable debug flag */
#ifdef	DEBUG
static unsigned		afe_debug = AFE_DWARN;
#endif

/* table of supported devices */
static afe_card_t afe_cards[] = {

	/*
	 * ADMtek Centaur and Comet
	 */
	{ 0x1317, 0x0981, "ADMtek AL981", AFE_MODEL_COMET },
	{ 0x1317, 0x0985, "ADMtek AN983", AFE_MODEL_CENTAUR },
	{ 0x1317, 0x1985, "ADMtek AN985", AFE_MODEL_CENTAUR },
	{ 0x1317, 0x9511, "ADMtek ADM9511", AFE_MODEL_CENTAUR },
	{ 0x1317, 0x9513, "ADMtek ADM9513", AFE_MODEL_CENTAUR },
	/*
	 * Accton just relabels other companies' controllers
	 */
	{ 0x1113, 0x1216, "Accton EN5251", AFE_MODEL_CENTAUR },
	/*
	 * Models listed here.
	 */
	{ 0x10b7, 0x9300, "3Com 3CSOHO100B-TX", AFE_MODEL_CENTAUR },
	{ 0x1113, 0xec02, "SMC SMC1244TX", AFE_MODEL_CENTAUR },
	{ 0x10b8, 0x1255, "SMC SMC1255TX", AFE_MODEL_CENTAUR },
	{ 0x111a, 0x1020, "Siemens SpeedStream PCI 10/100",
	  AFE_MODEL_CENTAUR },
	{ 0x1113, 0x1207, "Accton EN1207F", AFE_MODEL_CENTAUR },
	{ 0x1113, 0x2242, "Accton EN2242", AFE_MODEL_CENTAUR },
	{ 0x1113, 0x2220, "Accton EN2220", AFE_MODEL_CENTAUR },
	{ 0x1113, 0x9216, "3M VOL-N100VF+TX", AFE_MODEL_CENTAUR },
	{ 0x1317, 0x0574, "Linksys LNE100TX", AFE_MODEL_CENTAUR },
	{ 0x1317, 0x0570, "Network Everywhere NC100", AFE_MODEL_CENTAUR },
	{ 0x1385, 0x511a, "Netgear FA511", AFE_MODEL_CENTAUR },
	{ 0x13d1, 0xab02, "AboCom FE2500", AFE_MODEL_CENTAUR },
	{ 0x13d1, 0xab03, "AboCom PCM200", AFE_MODEL_CENTAUR },
	{ 0x13d1, 0xab08, "AboCom FE2500MX", AFE_MODEL_CENTAUR },
	{ 0x1414, 0x0001, "Microsoft MN-120", AFE_MODEL_CENTAUR },
	{ 0x16ec, 0x00ed, "U.S. Robotics USR997900", AFE_MODEL_CENTAUR },
	{ 0x1734, 0x100c, "Fujitsu-Siemens D1961 Onboard Ethernet",
	  AFE_MODEL_CENTAUR },
	{ 0x1737, 0xab08, "Linksys PCMPC200", AFE_MODEL_CENTAUR },
	{ 0x1737, 0xab09, "Linksys PCM200", AFE_MODEL_CENTAUR },
	{ 0x17b3, 0xab08, "Hawking PN672TX", AFE_MODEL_CENTAUR },
};

static uint32_t afe_txthresh[] = {
	AFE_NAR_TR_72,	/* 72 bytes (10Mbps), 128 bytes (100Mbps) */
	AFE_NAR_TR_96,	/* 96 bytes (10Mbps), 256 bytes (100Mbps) */
	AFE_NAR_TR_128,	/* 128 bytes (10Mbps), 512 bytes (100Mbps) */
	AFE_NAR_TR_160,	/* 160 bytes (10Mbps), 1024 bytes (100Mbps) */
	AFE_NAR_SF	/* store and forward */
};
#define	AFE_MAX_TXTHRESH	(sizeof (afe_txthresh)/sizeof (uint32_t))

/*
 * Function prototypes
 */
static int	afe_attach(dev_info_t *, ddi_attach_cmd_t);
static int	afe_detach(dev_info_t *, ddi_detach_cmd_t);
static int	afe_resume(gld_mac_info_t *);
static int	afe_set_mac_addr(gld_mac_info_t *, unsigned char *);
static int	afe_set_multicast(gld_mac_info_t *, unsigned char *, int);
static int	afe_set_promiscuous(gld_mac_info_t *, int);
static int	afe_send(gld_mac_info_t *, mblk_t *mp);
static int	afe_get_stats(gld_mac_info_t *, struct gld_stats *);
static int	afe_start(gld_mac_info_t *);
static int	afe_stop(gld_mac_info_t *);
static int	afe_ioctl(gld_mac_info_t *, queue_t *, mblk_t *);
static unsigned	afe_intr(gld_mac_info_t *);
static int	afe_reset(gld_mac_info_t *);
static int	afe_startmac(afe_t *);
static int	afe_stopmac(afe_t *);
static int	afe_mcadd(afe_t *, unsigned char *);
static int	afe_mcdelete(afe_t *, unsigned char *);
static int	afe_allocrings(afe_t *);
static void	afe_freerings(afe_t *);
static afe_buf_t	*afe_getbuf(afe_t *, int);
static void	afe_freebuf(afe_buf_t *);
static unsigned	afe_etherhashbe(uchar_t *);
static int	afe_msgsize(mblk_t *);
static void	afe_miocack(queue_t *, mblk_t *, uint8_t, int, int);
static void	afe_error(dev_info_t *, char *, ...);
static void	afe_verror(dev_info_t *, int, char *, va_list);
static void	afe_sromdelay(afe_t *);
static ushort	afe_sromwidth(afe_t *);
static ushort	afe_readsromword(afe_t *, unsigned);
static void	afe_readsrom(afe_t *, unsigned, unsigned, char *);
static void	afe_getfactaddr(afe_t *, uchar_t *);
static int	afe_miireadbit(afe_t *);
static void	afe_miiwritebit(afe_t *, int);
static void	afe_miitristate(afe_t *);
static unsigned	afe_miiread(afe_t *, int, int);
static void	afe_miiwrite(afe_t *, int, int, ushort);
static unsigned	afe_miireadgeneral(afe_t *, int, int);
static void	afe_miiwritegeneral(afe_t *, int, int, ushort);
static unsigned	afe_miireadcomet(afe_t *, int, int);
static void	afe_miiwritecomet(afe_t *, int, int, ushort);
static void	afe_phyinit(afe_t *);
static void	afe_reportlink(afe_t *);
static void	afe_checklink(afe_t *);
static void	afe_checklinkcomet(afe_t *);
static void	afe_checklinkcentaur(afe_t *);
static void	afe_checklinkmii(afe_t *);
static void	afe_disableinterrupts(afe_t *);
static void	afe_enableinterrupts(afe_t *);
static void	afe_reclaim(afe_t *);
static mblk_t *	afe_read(afe_t *, int, unsigned);
static int	afe_ndaddbytes(mblk_t *, char *, int);
static int	afe_ndaddstr(mblk_t *, char *, int);
static void	afe_ndparsestring(mblk_t *, char *, int);
static int	afe_ndparselen(mblk_t *);
static int	afe_ndparseint(mblk_t *);
static void	afe_ndget(afe_t *, queue_t *, mblk_t *);
static void	afe_ndset(afe_t *, queue_t *, mblk_t *);
static void	afe_ndfini(afe_t *);
static void	afe_ndinit(afe_t *);

#ifdef	DEBUG
static void	afe_dprintf(afe_t *, const char *, int, char *, ...);
#endif

#define	KIOIP	KSTAT_INTR_PTR(afep->afe_intrstat)

/*
 * Stream information
 */
static struct module_info afe_module_info = {
	AFE_IDNUM,		/* mi_idnum */
	AFE_IDNAME,		/* mi_idname */
	AFE_MINPSZ,		/* mi_minpsz */
	AFE_MAXPSZ,		/* mi_maxpsz */
	AFE_HIWAT,		/* mi_hiwat */
	AFE_LOWAT		/* mi_lowat */
};

static struct qinit afe_rinit = {
	NULL,			/* qi_putp */
	gld_rsrv,		/* qi_srvp */
	gld_open,		/* qi_qopen */
	gld_close,		/* qi_qclose */
	NULL,			/* qi_qadmin */
	&afe_module_info,	/* qi_minfo */
	NULL			/* qi_mstat */
};

static struct qinit afe_winit = {
	gld_wput,		/* qi_putp */
	gld_wsrv,		/* qi_srvp */
	NULL,			/* qi_qopen */
	NULL,			/* qi_qclose */
	NULL,			/* qi_qadmin */
	&afe_module_info,	/* qi_minfo */
	NULL			/* qi_mstat */
};

static struct streamtab afe_streamtab = {
	&afe_rinit,		/* st_rdinit */
	&afe_winit,		/* st_wrinit */
	NULL,			/* st_muxrinit */
	NULL			/* st_muxwinit */
};

/*
 * Character/block operations.
 */
static struct cb_ops afe_cbops = {
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
	&afe_streamtab,				/* cb_stream */
	D_MP,					/* cb_flag */
	CB_REV,					/* cb_rev */
	nodev,					/* cb_aread */
	nodev					/* cb_awrite */
};

/*
 * Device operations.
 */
static struct dev_ops afe_devops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	gld_getinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	afe_attach,		/* devo_attach */
	afe_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&afe_cbops,		/* devo_cb_ops */
	NULL,			/* devo_bus_ops */
	ddi_power		/* devo_power */
};

/*
 * Module linkage information.
 */
#define	AFE_IDENT	"AFE Fast Ethernet"
static char afe_ident[MODMAXNAMELEN];
static char *afe_version;

static struct modldrv afe_modldrv = {
	&mod_driverops,			/* drv_modops */
	afe_ident,			/* drv_linkinfo */
	&afe_devops			/* drv_dev_ops */
};

static struct modlinkage afe_modlinkage = {
	MODREV_1,		/* ml_rev */
	{ &afe_modldrv, NULL }	/* ml_linkage */
};

/*
 * Device attributes.
 */
static ddi_device_acc_attr_t afe_devattr = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};

static ddi_device_acc_attr_t afe_bufattr = {
	DDI_DEVICE_ATTR_V0,
	DDI_NEVERSWAP_ACC,
	DDI_STRICTORDER_ACC
};

static ddi_dma_attr_t afe_dma_attr = {
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
static uchar_t afe_broadcast_addr[ETHERADDRL] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/*
 * DDI entry points.
 */
int
_init(void)
{
	char	*rev = "$Revision: 1.13 $";
	char	*ident = afe_ident;

	/* this technique works for both RCS and SCCS */
	strcpy(ident, AFE_IDENT " v");
	ident += strlen(ident);
	afe_version = ident;
	while (*rev) {
		if (strchr("0123456789.", *rev)) {
			*ident = *rev;
			ident++;
			*ident = 0;
		}
		rev++;
	}

	return (mod_install(&afe_modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&afe_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&afe_modlinkage, modinfop));
}

static int
afe_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	gld_mac_info_t		*macinfo;
	afe_t			*afep;
	int			inst = ddi_get_instance(dip);
	ddi_acc_handle_t	pci;
	ushort			venid;
	ushort			devid;
	ushort			svid;
	ushort			ssid;
	ushort			revid;
	ushort			cachesize;
	afe_card_t		*cardp;
	int			i;
	char			buf[16];

	switch (cmd) {
	case DDI_RESUME:
		macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
		if (macinfo == NULL) {
			return (DDI_FAILURE);
		}
		return (afe_resume(macinfo));

	case DDI_ATTACH:
		break;

	default:
		return (DDI_FAILURE);
	}

	/* this card is a bus master, reject any slave-only slot */
	if (ddi_slaveonly(dip) == DDI_SUCCESS) {
		afe_error(dip, "slot does not support PCI bus-master");
		return (DDI_FAILURE);
	}
	/* PCI devices shouldn't generate hilevel interrupts */
	if (ddi_intr_hilevel(dip, 0) != 0) {
		afe_error(dip, "hilevel interrupts not supported");
		return (DDI_FAILURE);
	}
	if (pci_config_setup(dip, &pci) != DDI_SUCCESS) {
		afe_error(dip, "unable to setup PCI config handle");
		return (DDI_FAILURE);
	}

	venid = pci_config_get16(pci, AFE_PCI_VID);
	devid = pci_config_get16(pci, AFE_PCI_DID);
	revid = pci_config_get16(pci, AFE_PCI_RID);
	svid = pci_config_get16(pci, AFE_PCI_SVID);
	ssid = pci_config_get16(pci, AFE_PCI_SSID);

	DBG(AFE_DPCI, "mingnt %x", pci_config_get8(pci, AFE_PCI_MINGNT));
	DBG(AFE_DPCI, "maxlat %x", pci_config_get8(pci, AFE_PCI_MAXLAT));

	/*
	 * FIXME: ADMtek boards seem to misprogram themselves with bogus
	 * timings, which do not seem to work properly on SPARC.  We
	 * reprogram them zero (but only if they appear to be broken),
	 * which seems to at least work.  Its unclear that this is a
	 * legal or wise practice to me, but it certainly works better
	 * than the original values.  (I would love to hear
	 * suggestions for better values, or a better strategy.)
	 */
	if ((pci_config_get8(pci, AFE_PCI_MINGNT) == 0xff) &&
	    (pci_config_get8(pci, AFE_PCI_MAXLAT) == 0xff)) {
		DBG(AFE_DPCI, "clearing MINGNT and MAXLAT");
		pci_config_put8(pci, AFE_PCI_MINGNT, 0);
		pci_config_put8(pci, AFE_PCI_MAXLAT, 0);
		DBG(AFE_DPCI, "mingnt %x",
		    pci_config_get8(pci, AFE_PCI_MINGNT));
		DBG(AFE_DPCI, "maxlat %x",
		    pci_config_get8(pci, AFE_PCI_MAXLAT));
	}

	/*
	 * the last entry in the card table matches every possible
	 * card, so the for-loop always terminates properly.
	 */
	cardp = NULL;
	for (i = 0; i < (sizeof (afe_cards) / sizeof (afe_card_t)); i++) {
		if ((svid == afe_cards[i].card_venid) &&
		    (ssid == afe_cards[i].card_devid)) {
			cardp = &afe_cards[i];
			break;
		}
		if ((venid == afe_cards[i].card_venid) &&
		    (devid == afe_cards[i].card_devid)) {
			cardp = &afe_cards[i];
			/* keep going, a subsystem might match instead */
		}
	}

	if (cardp == NULL) {
		afe_error(dip, "Unable to identify PCI card");
		pci_config_teardown(&pci);
		return (DDI_FAILURE);
	}

	if (ddi_prop_update_string(DDI_DEV_T_NONE, dip, "model",
	    cardp->card_cardname) != DDI_PROP_SUCCESS) {
		afe_error(dip, "Unable to create model property");
		pci_config_teardown(&pci);
                return (DDI_FAILURE);
        }

	/*
	 * Grab the PCI cachesize -- we use this to program the
	 * cache-optimization bus access bits.
	 */
	cachesize = pci_config_get8(pci, AFE_PCI_CLS);

	if ((macinfo = gld_mac_alloc(dip)) == NULL) {
		afe_error(dip, "Unable to allocate macinfo");
		pci_config_teardown(&pci);
		return (DDI_FAILURE);
	}

	/* this cannot fail */
	afep = (afe_t *)kmem_zalloc(sizeof (afe_t), KM_SLEEP);
	afep->afe_macinfo = macinfo;

	macinfo->gldm_private =		(void *)afep;
	macinfo->gldm_reset =		afe_reset;
	macinfo->gldm_start =		afe_start;
	macinfo->gldm_stop =		afe_stop;
	macinfo->gldm_set_mac_addr =	afe_set_mac_addr;
	macinfo->gldm_set_multicast =	afe_set_multicast;
	macinfo->gldm_set_promiscuous =	afe_set_promiscuous;
	macinfo->gldm_get_stats =	afe_get_stats;
	macinfo->gldm_send =		afe_send;
	macinfo->gldm_intr =		afe_intr;
	macinfo->gldm_ioctl =		afe_ioctl;

	macinfo->gldm_ident =		cardp->card_cardname;
	macinfo->gldm_type =		DL_ETHER;
	macinfo->gldm_minpkt =		0;
	macinfo->gldm_maxpkt =		ETHERMTU;
	macinfo->gldm_addrlen =		ETHERADDRL;
	macinfo->gldm_saplen =		-2;
	macinfo->gldm_broadcast_addr =	afe_broadcast_addr;
	macinfo->gldm_vendor_addr =	afep->afe_factaddr;
	macinfo->gldm_devinfo =		dip;
	macinfo->gldm_ppa =		inst;

	/* get the interrupt block cookie */
        if (ddi_get_iblock_cookie(dip, 0, &macinfo->gldm_cookie)
            != DDI_SUCCESS) {
		afe_error(dip, "ddi_get_iblock_cookie failed");
		pci_config_teardown(&pci);
		kmem_free(afep, sizeof (afe_t));
		gld_mac_free(macinfo);
		return (DDI_FAILURE);
	}

	ddi_set_driver_private(dip, (caddr_t)macinfo);

	/*
	 * Set up our per-instance configuration.
	 */
	afep->afe_dip = dip;
	afep->afe_cardp = cardp;
	afep->afe_phyaddr = -1;
	afep->afe_cachesize = cachesize;
	afep->afe_numbufs = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    DDI_PROP_CANSLEEP, "buffers", AFE_NUMBUFS);

	/* default properties */
	afep->afe_adv_aneg = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    0, "adv_autoneg_cap", 1);
	afep->afe_adv_100T4 = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    0, "adv_100T4_cap", 1);
	afep->afe_adv_100fdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    0, "adv_100fdx_cap", 1);
	afep->afe_adv_100hdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    0, "adv_100hdx_cap", 1);
	afep->afe_adv_10fdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    0, "adv_10fdx_cap", 1);
	afep->afe_adv_10hdx = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    0, "adv_10hdx_cap", 1);

	/*
	 * Legacy properties.  These override newer properties, only
	 * for ease of implementation.
	 */
	switch (ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "speed", 0)) {
	case 100:
		afep->afe_adv_10fdx = 0;
		afep->afe_adv_10hdx = 0;
		break;
	case 10:
		afep->afe_adv_100fdx = 0;
		afep->afe_adv_100hdx = 0;
		break;
	}

	switch (ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "full-duplex", -1)) {
	case 1:
		afep->afe_adv_10hdx = 0;
		afep->afe_adv_100hdx = 0;
		break;
	case 0:
		afep->afe_adv_10fdx = 0;
		afep->afe_adv_100fdx = 0;
		break;
	}

	afep->afe_forcefiber = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    0, "fiber", 0);

	/*
	 * XXX: Add in newer MII properties here.
	 */

	DBG(AFE_DPCI, "PCI vendor id = %x", venid);
	DBG(AFE_DPCI, "PCI device id = %x", devid);
	DBG(AFE_DPCI, "PCI revision id = %x", revid);
	DBG(AFE_DPCI, "PCI cachesize = %d", cachesize);
	DBG(AFE_DPCI, "PCI COMM = %x", pci_config_get8(pci, AFE_PCI_COMM));
	DBG(AFE_DPCI, "PCI STAT = %x", pci_config_get8(pci, AFE_PCI_STAT));

	mutex_init(&afep->afe_buflock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);
	mutex_init(&afep->afe_xmtlock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);
	mutex_init(&afep->afe_intrlock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);

	afe_ndinit(afep);

	/*
	 * Initialize interrupt kstat.
	 */
	sprintf(buf, "afec%d", inst);
	afep->afe_intrstat = kstat_create("afe", inst, buf, "controller",
	    KSTAT_TYPE_INTR, 1, KSTAT_FLAG_PERSISTENT);
	if (afep->afe_intrstat) {
		kstat_install(afep->afe_intrstat);
	}

	/*
	 * Enable bus master, IO space, and memory space accesses.
	 */
	pci_config_put16(pci, AFE_PCI_COMM,
	    pci_config_get16(pci, AFE_PCI_COMM) |
	    AFE_PCI_BME | AFE_PCI_MAE | AFE_PCI_IOE);

	/* we're done with this now, drop it */
	pci_config_teardown(&pci);

	/*
	 * Map in the device registers.
	 */
	ddi_dev_regsize(dip, 1, &afep->afe_regsize);
	if (ddi_regs_map_setup(dip, 1, (caddr_t *)&afep->afe_regs,
	    0, 0, &afe_devattr, &afep->afe_regshandle)) {
		afe_error(dip, "ddi_regs_map_setup failed");
		goto failed;
	}

	/* Stop the board. */
	CLRBIT(afep, AFE_CSR_NAR, AFE_TX_ENABLE | AFE_RX_ENABLE);

	/* Turn off all interrupts for now. */
	afe_disableinterrupts(afep);

	/*
	 * Allocate DMA resources (descriptor rings and buffers).
	 */
	if (afe_allocrings(afep) != DDI_SUCCESS) {
		afe_error(dip, "unable to allocate DMA resources");
		goto failed;
	}

	/* Add the broadcast address to multicast table. */
	(void) afe_mcadd(afep, afe_broadcast_addr);

	/* Reset the chip. */
	afe_reset(macinfo);

	/* FIXME: insert hardware initializations here */

	/* Determine the number of address bits to our EEPROM. */
	afep->afe_sromwidth = afe_sromwidth(afep);

	/*
	 * Get the factory ethernet address.  This becomes the current
	 * ethernet address (it can be overridden later via ifconfig).
	 * FIXME: consider allowing this to be tunable via a property
	 */
	afe_getfactaddr(afep, afep->afe_factaddr);
	afep->afe_promisc = GLD_MAC_PROMISC_NONE;

	if (ddi_add_intr(dip, 0, NULL, NULL, gld_intr,
	    (void *)macinfo) != DDI_SUCCESS) {
		afe_error(dip, "unable to add interrupt");
		goto failed;
	}

	/* FIXME: do the power management stuff */

	if (gld_register(dip, AFE_IDNAME, macinfo) == DDI_SUCCESS) {
		return (DDI_SUCCESS);
	}

	/* failed to register with GLD */

failed:
	if (macinfo->gldm_cookie != NULL) {
		ddi_remove_intr(dip, 0, macinfo->gldm_cookie);
	}
	if (afep->afe_intrstat) {
		kstat_delete(afep->afe_intrstat);
	}
	afe_ndfini(afep);
	mutex_destroy(&afep->afe_buflock);
	mutex_destroy(&afep->afe_intrlock);
	mutex_destroy(&afep->afe_xmtlock);
	if (afep->afe_desc_dmahandle != NULL) {
		afe_freerings(afep);
	}
	if (afep->afe_regshandle != NULL) {
		ddi_regs_map_free(&afep->afe_regshandle);
	}
	kmem_free(afep, sizeof (afe_t));
	gld_mac_free(macinfo);
	return (DDI_FAILURE);
}

static int
afe_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	gld_mac_info_t	*macinfo;
	afe_t		*afep;

	macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	if (macinfo == NULL) {
		afe_error(dip, "no soft state in detach!");
		return (DDI_FAILURE);
	}
	afep = (afe_t *)macinfo->gldm_private;

	switch (cmd) {
	case DDI_DETACH:
		if (gld_unregister(macinfo) != DDI_SUCCESS) {
			return (DDI_FAILURE);
		}

		/* make sure hardware is quiesced */
		afe_stop(macinfo);

		/* clean up and shut down device */
		ddi_remove_intr(dip, 0, macinfo->gldm_cookie);

		/* FIXME: delete properties */

		/* free up any left over buffers or DMA resources */
		if (afep->afe_desc_dmahandle != NULL) {
			int	i;
			/* free up buffers first (reclaim) */
			for (i = 0; i < AFE_TXRING; i++) {
				if (afep->afe_txbufs[i]) {
					afe_freebuf(afep->afe_txbufs[i]);
					afep->afe_txbufs[i] = NULL;
				}
			}
			for (i = 0; i < AFE_RXRING; i++) {
				if (afep->afe_rxbufs[i]) {
					afe_freebuf(afep->afe_rxbufs[i]);
					afep->afe_rxbufs[i] = NULL;
				}
			}
			/* then free up DMA resourcess */
			afe_freerings(afep);
		}

		/* delete kstats */
		if (afep->afe_intrstat) {
			kstat_delete(afep->afe_intrstat);
		}

		afe_ndfini(afep);
		ddi_regs_map_free(&afep->afe_regshandle);
		mutex_destroy(&afep->afe_buflock);
		mutex_destroy(&afep->afe_intrlock);
		mutex_destroy(&afep->afe_xmtlock);

		kmem_free(afep, sizeof (afe_t));
		gld_mac_free(macinfo);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		/* quiesce the hardware */
		mutex_enter(&afep->afe_intrlock);
		mutex_enter(&afep->afe_xmtlock);
		afep->afe_flags |= AFE_SUSPENDED;
		afe_stopmac(afep);
		mutex_exit(&afep->afe_xmtlock);
		mutex_exit(&afep->afe_intrlock);
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
afe_ioctl(gld_mac_info_t *macinfo, queue_t *wq, mblk_t *mp)
{
	afe_t *afep = (afe_t *)macinfo->gldm_private;
	switch (IOC_CMD(mp)) {

	case NDIOC_GET:
		afe_ndget(afep, wq, mp);
		break;

	case NDIOC_SET:
		afe_ndset(afep, wq, mp);
		break;

	default:
		afe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		break;
	}
	return (GLD_SUCCESS);
}

static void
afe_setrxfilt(afe_t *afep)
{
	unsigned rxen, pa0, pa1;

	if (afep->afe_flags & AFE_SUSPENDED) {
		/* don't touch a suspended interface */
		return;
	}

	rxen = GETCSR(afep, AFE_CSR_NAR) & AFE_RX_ENABLE;

	/* stop receiver */
	if (rxen) {
		CLRBIT(afep, AFE_CSR_NAR, rxen);
	}

	/* program promiscuous mode */
	switch (afep->afe_promisc) {
	case GLD_MAC_PROMISC_PHYS:
		DBG(AFE_DMACID, "setting promiscuous");
		CLRBIT(afep, AFE_CSR_NAR, AFE_RX_MULTI);
		SETBIT(afep, AFE_CSR_NAR, AFE_RX_PROMISC);
		break;
	case GLD_MAC_PROMISC_MULTI:
		DBG(AFE_DMACID, "setting allmulti");
		CLRBIT(afep, AFE_CSR_NAR, AFE_RX_PROMISC);
		SETBIT(afep, AFE_CSR_NAR, AFE_RX_MULTI);
		break;
	case GLD_MAC_PROMISC_NONE:
		CLRBIT(afep, AFE_CSR_NAR, AFE_RX_PROMISC | AFE_RX_MULTI);
		break;
	}

	/* program mac address */
	pa0 = (afep->afe_curraddr[3] << 24) | (afep->afe_curraddr[2] << 16) |
	    (afep->afe_curraddr[1] << 8) | afep->afe_curraddr[0];
	pa1 = (afep->afe_curraddr[5] << 8) | afep->afe_curraddr[4];

	DBG(AFE_DMACID, "programming PAR0 with %x", pa0);
	DBG(AFE_DMACID, "programming PAR1 with %x", pa1);
	PUTCSR(afep, AFE_CSR_PAR0, pa0);
	PUTCSR(afep, AFE_CSR_PAR1, pa1);
	if (rxen) {
		SETBIT(afep, AFE_CSR_NAR, rxen);
	}

	/* program multicast filter */
	DBG(AFE_DMACID, "programming MAR0 = %x", afep->afe_mctab[0]);
	DBG(AFE_DMACID, "programming MAR1 = %x", afep->afe_mctab[1]);
	PUTCSR(afep, AFE_CSR_MAR0, afep->afe_mctab[0]);
	PUTCSR(afep, AFE_CSR_MAR1, afep->afe_mctab[1]);

	/* restart receiver */
	if (rxen) {
		SETBIT(afep, AFE_CSR_NAR, rxen);
	}
}

static int
afe_mcadd(afe_t *afep, unsigned char *macaddr)
{
	unsigned	hash;
	int		changed = 0;

	hash = afe_etherhashbe(macaddr) % AFE_MCHASH;

	if ((afep->afe_mccount[hash]) == 0) {
		afep->afe_mctab[hash / sizeof (unsigned)] |=
		    (1 << (hash % sizeof (unsigned)));
		changed++;
	}
	afep->afe_mccount[hash]++;
	return (changed);
}

static int
afe_mcdelete(afe_t *afep, unsigned char *macaddr)
{
	unsigned	hash;
	int		changed = 0;

	hash = afe_etherhashbe(macaddr) % AFE_MCHASH;

	if ((afep->afe_mccount[hash]) == 1) {
		afep->afe_mctab[hash / sizeof (unsigned)] &=
		    ~(1 << (hash % sizeof (unsigned)));
		changed++;
	}
	afep->afe_mccount[hash]--;

	return (changed);
}

static int
afe_set_multicast(gld_mac_info_t *macinfo, unsigned char *macaddr, int flag)
{
	afe_t		*afep = (afe_t *)macinfo->gldm_private;
	int		changed;

	/* exclusive access to the card while we reprogram it */
	mutex_enter(&afep->afe_xmtlock);

	switch (flag) {
	case GLD_MULTI_ENABLE:
		changed = afe_mcadd(afep, macaddr);
		break;
	case GLD_MULTI_DISABLE:
		changed = afe_mcdelete(afep, macaddr);
		break;
	}
	
	if (changed) {
		afe_setrxfilt(afep);
	}

	/* also make sure we drop the right locks */
	mutex_exit(&afep->afe_xmtlock);

	return (GLD_SUCCESS);
}

static int
afe_set_promiscuous(gld_mac_info_t *macinfo, int flag)
{
	afe_t		*afep = (afe_t *)macinfo->gldm_private;

	/* exclusive access to the card while we reprogram it */
	mutex_enter(&afep->afe_xmtlock);
	/* save current promiscuous mode state for replay in resume */
	afep->afe_promisc = flag;

	afe_setrxfilt(afep);

	mutex_exit(&afep->afe_xmtlock);

	return (GLD_SUCCESS);
}

static int
afe_set_mac_addr(gld_mac_info_t *macinfo, unsigned char *macaddr)
{
	afe_t		*afep = (afe_t *)macinfo->gldm_private;

	/* exclusive access to the card while we reprogram it */
	mutex_enter(&afep->afe_xmtlock);

	/*
	 * Save new mac address.  Perhaps we should check it for
	 * illegal mac addresses?  For now we don't bother.
	 */
	bcopy(macaddr, afep->afe_curraddr, ETHERADDRL);

	/*
	 * Program our ethernet address unconditionally.  Usually this
	 * will be the same as the factory address.
	 */
	afe_setrxfilt(afep);

	mutex_exit(&afep->afe_xmtlock);

	return (GLD_SUCCESS);
}

/*
 * Hardware management.
 */
static int
afe_resetmac(afe_t *afep)
{
	int		i;
	unsigned	val;

	ASSERT(mutex_owned(&afep->afe_intrlock));
	ASSERT(mutex_owned(&afep->afe_xmtlock));

	DBG(AFE_DCHATTY, "resetting!");
	SETBIT(afep, AFE_CSR_PAR, AFE_RESET);
	for (i = 1; i < 10; i++) {
		drv_usecwait(5);
		val = GETCSR(afep, AFE_CSR_PAR);
		if (!(val & AFE_RESET)) {
			break;
		}
	}
	if (i == 10) {
		afe_error(afep->afe_dip, "timed out waiting for reset!");
		return (GLD_FAILURE);
	}

	/* FIXME: possibly set some other regs here, e.g. arbitration. */
	/* initialize busctl register */
	switch (AFE_MODEL(afep)) {
	case AFE_MODEL_COMET:

		/* clear all the cache alignment bits */
		CLRBIT(afep, AFE_CSR_PAR, AFE_CALIGN_32);

		/* then set the cache alignment if its supported */
		switch (afep->afe_cachesize) {
		case 8:
			SETBIT(afep, AFE_CSR_PAR, AFE_CALIGN_8);
			break;
		case 16:
			SETBIT(afep, AFE_CSR_PAR, AFE_CALIGN_16);
			break;
		case 32:
			SETBIT(afep, AFE_CSR_PAR, AFE_CALIGN_32);
			break;
		}

		/* unconditional 32-word burst */
		SETBIT(afep, AFE_CSR_PAR, AFE_BURST_32);
		break;

	case AFE_MODEL_CENTAUR:
		PUTCSR(afep, AFE_CSR_PAR, AFE_TXHIPRI | AFE_RXFIFO_100);
		break;
	}

	/* enable transmit underrun auto-recovery */
	SETBIT(afep, AFE_CSR_CR, AFE_CR_TXURAUTOR);

	return (GLD_SUCCESS);
}

static int
afe_reset(gld_mac_info_t *macinfo)
{
	afe_t		*afep = (afe_t *)macinfo->gldm_private;
	int		rv;

	mutex_enter(&afep->afe_intrlock);
	mutex_enter(&afep->afe_xmtlock);

	rv = afe_resetmac(afep);

	mutex_exit(&afep->afe_xmtlock);
	mutex_exit(&afep->afe_intrlock);
	return (rv);
}

static int
afe_resume(gld_mac_info_t *macinfo)
{
	afe_t	*afep;
	if (macinfo == NULL) {
		return (DDI_FAILURE);
	}
	afep = (afe_t *)macinfo->gldm_private;

	mutex_enter(&afep->afe_intrlock);
	mutex_enter(&afep->afe_xmtlock);

	afep->afe_flags &= ~AFE_SUSPENDED;

	/* reset the chip */
	if (afe_resetmac(afep) != GLD_SUCCESS) {
		afe_error(afep->afe_dip, "unable to resume chip!");
		afep->afe_flags |= AFE_SUSPENDED;
		mutex_exit(&afep->afe_intrlock);
		mutex_exit(&afep->afe_xmtlock);
		return (DDI_SUCCESS);
	}

	/* restore rx filter (macaddr, promisc, multicast table) */
	afe_setrxfilt(afep);

	/* start the chip */
	if (afep->afe_flags & AFE_RUNNING) {
		if (afe_startmac(afep) != GLD_SUCCESS) {
			afe_error(afep->afe_dip, "unable to restart mac!");
			afep->afe_flags |= AFE_SUSPENDED;
			mutex_exit(&afep->afe_intrlock);
			mutex_exit(&afep->afe_xmtlock);
			return (DDI_SUCCESS);
		}
	}

	/* drop locks */
	mutex_exit(&afep->afe_xmtlock);
	mutex_exit(&afep->afe_intrlock);

	return (DDI_SUCCESS);
}

/*
 * Serial EEPROM access - derived from the FreeBSD implementation.
 */
/*ARGSUSED*/
static void
afe_sromdelay(afe_t *afep)
{
	/*
	 * this is the finest granularity we have -- we only need at
	 * most a quarter usec; the Linux and FreeBSD drivers use
	 * bus cycles to insert the necessary time requirements, so
	 * we try the same trick.
	 */
	drv_usecwait(1);
	/* GETCSR(afep, AFE_CSR_SPR); */
}

static ushort
afe_sromwidth(afe_t *afep)
{
	int		i;
	int		eeread = AFE_SROM_READ | AFE_SROM_SEL | AFE_SROM_CHIP;
	int		addrlen = 8;
	int		readcmd;

	readcmd = AFE_SROM_READCMD;

	PUTCSR(afep, AFE_CSR_SPR, eeread & ~AFE_SROM_CHIP);
	PUTCSR(afep, AFE_CSR_SPR, eeread);

	/* command bits first */
	for (i = 2; i >= 0; i--) {
		short val = (readcmd & (1 << i)) ?  AFE_SROM_DIN : 0;
		PUTCSR(afep, AFE_CSR_SPR, eeread | val);
		afe_sromdelay(afep);
		PUTCSR(afep, AFE_CSR_SPR, eeread | val | AFE_SROM_CLOCK);
		afe_sromdelay(afep);
	}

	PUTCSR(afep, AFE_CSR_SPR, eeread);

	for (addrlen = 1; addrlen <= 12; addrlen++) {
		PUTCSR(afep, AFE_CSR_SPR, eeread | AFE_SROM_CLOCK);
		afe_sromdelay(afep);
		if (!(GETCSR(afep, AFE_CSR_SPR) & AFE_SROM_DOUT)) {
			PUTCSR(afep, AFE_CSR_SPR, eeread);
			afe_sromdelay(afep);
			break;
		}
		PUTCSR(afep, AFE_CSR_SPR, eeread);
		afe_sromdelay(afep);
	}

	/* turn off accesses to the EEPROM */
	PUTCSR(afep, AFE_CSR_SPR, eeread &~ AFE_SROM_CHIP);

	DBG(AFE_DSROM, "detected srom width = %d bits", addrlen);

	return ((addrlen < 4 || addrlen > 12) ? 6 : addrlen);
}

/*
 * The words in EEPROM are stored in little endian order.  We
 * shift bits out in big endian order, though.  This requires
 * a byte swap on some platforms.
 */
static ushort
afe_readsromword(afe_t *afep, unsigned romaddr)
{
	int		i;
	ushort		word = 0;
	ushort		retval;
	int		eeread = AFE_SROM_READ | AFE_SROM_SEL | AFE_SROM_CHIP;
	int		addrlen;
	int		readcmd;
	uchar_t		*ptr;

	addrlen = afep->afe_sromwidth;
	readcmd = (AFE_SROM_READCMD << addrlen) | romaddr;

	if (romaddr >= (1 << addrlen)) {
		/* too big to fit! */
		return (0);
	}

	PUTCSR(afep, AFE_CSR_SPR, eeread & ~AFE_SROM_CHIP);
	PUTCSR(afep, AFE_CSR_SPR, eeread);

	/* command and address bits */
	for (i = 4 + addrlen; i >= 0; i--) {
		short val = (readcmd & (1 << i)) ?  AFE_SROM_DIN : 0;
		PUTCSR(afep, AFE_CSR_SPR, eeread | val);
		afe_sromdelay(afep);
		PUTCSR(afep, AFE_CSR_SPR, eeread | val | AFE_SROM_CLOCK);
		afe_sromdelay(afep);
	}

	PUTCSR(afep, AFE_CSR_SPR, eeread);

	for (i = 0; i < 16; i++) {
		PUTCSR(afep, AFE_CSR_SPR, eeread | AFE_SROM_CLOCK);
		afe_sromdelay(afep);
		word <<= 1;
		if (GETCSR(afep, AFE_CSR_SPR) & AFE_SROM_DOUT) {
			word |= 1;
		}
		PUTCSR(afep, AFE_CSR_SPR, eeread);
		afe_sromdelay(afep);
	}

	/* turn off accesses to the EEPROM */
	PUTCSR(afep, AFE_CSR_SPR, eeread &~ AFE_SROM_CHIP);

	/*
	 * Fix up the endianness thing.  Note that the values
	 * are stored in little endian format on the SROM.
	 */
	ptr = (uchar_t *)&word;
	retval = (ptr[1] << 8) | ptr[0];
	return (retval);
}

static void
afe_readsrom(afe_t *afep, unsigned romaddr, unsigned len, char *dest)
{
	int	i;
	ushort	word;
	ushort	*ptr = (ushort *)dest;
	for (i = 0; i < len; i++) {
		word = afe_readsromword(afep, romaddr + i);
		*ptr = word;
		ptr++;
	}
}

static void
afe_getfactaddr(afe_t *afep, uchar_t *eaddr)
{
	afe_readsrom(afep, AFE_SROM_ENADDR, ETHERADDRL / 2, (char *)eaddr);
	DBG(AFE_DMACID,
	    "factory ethernet address = %02x:%02x:%02x:%02x:%02x:%02x",
	    eaddr[0], eaddr[1], eaddr[2], eaddr[3], eaddr[4], eaddr[5]);
}

/*
 * MII management.
 */
static void
afe_phyinit(afe_t *afep)
{
	unsigned	phyaddr;
	unsigned	bmcr;
	unsigned	bmsr;
	unsigned	anar;
	unsigned	phyidr1;
	unsigned	phyidr2;
	unsigned	nosqe = 0;
	int		retries;
	int		force;
	int		fiber;
	int		cnt;
	int		validmode;

	/* ADMtek devices just use the PHY at address 1 */
	afep->afe_phyaddr = phyaddr = 1;

	phyidr1 = afe_miiread(afep, phyaddr, MII_REG_PHYIDR1);
	phyidr2 = afe_miiread(afep, phyaddr, MII_REG_PHYIDR2);
	if ((phyidr1 == 0x0022) &&
	    ((phyidr2 & 0xfff0) ==  0x5410)) {
		nosqe = 1;
		/* only 983B has fiber support */
		afep->afe_flags |= AFE_HASFIBER;
	}

	DBG(AFE_DPHY, "phy at %d: %x,%x", phyaddr, phyidr1, phyidr2);
	DBG(AFE_DPHY, "bmsr = %x", afe_miiread(afep,
	    afep->afe_phyaddr, MII_REG_BMSR));
	DBG(AFE_DPHY, "anar = %x", afe_miiread(afep,
	    afep->afe_phyaddr, MII_REG_ANAR));
	DBG(AFE_DPHY, "anlpar = %x", afe_miiread(afep,
	    afep->afe_phyaddr, MII_REG_ANLPAR));
	DBG(AFE_DPHY, "aner = %x", afe_miiread(afep,
	    afep->afe_phyaddr, MII_REG_ANER));

	DBG(AFE_DPHY, "resetting phy");

	/* we reset the phy block */
	afe_miiwrite(afep, phyaddr, MII_REG_BMCR, MII_BMCR_RESET);
	/*
	 * wait for it to complete -- 500usec is still to short to
	 * bother getting the system clock involved.
	 */
	drv_usecwait(500);
	for (retries = 0; retries < 10; retries++) {
		if (afe_miiread(afep, phyaddr, MII_REG_BMCR) &
		    MII_BMCR_RESET) {
			drv_usecwait(500);
			continue;
		}
		break;
	}
	if (retries == 100) {
		afe_error(afep->afe_dip, "timeout waiting for phy to reset");
		return;
	}

	DBG(AFE_DPHY, "phy reset complete");

	bmsr = afe_miiread(afep, phyaddr, MII_REG_BMSR);
	anar = afe_miiread(afep, phyaddr, MII_REG_ANAR);

	anar &= ~(MII_ANEG_100BT4 | MII_ANEG_100FDX | MII_ANEG_100HDX |
	    MII_ANEG_10FDX | MII_ANEG_10HDX);

	force = 0;
	fiber = 0;

	/* if fiber is being forced, and device supports fiber... */
	if (afep->afe_flags & AFE_HASFIBER) {

		uint16_t	mcr;

		DBG(AFE_DPHY, "device supports 100BaseFX");
		mcr = afe_miiread(afep, phyaddr, AFE_PHY_MCR);
		switch (afep->afe_forcefiber) {
		case 0:
			/* UTP Port */
			DBG(AFE_DPHY, "forcing twpair");
			mcr &= ~AFE_MCR_FIBER;
			fiber = 0;
			break;
		case 1:
			/* Fiber Port */
			force = 1;
			DBG(AFE_DPHY, "forcing 100BaseFX");
			mcr |= AFE_MCR_FIBER;
			bmcr = (MII_BMCR_SPEED | MII_BMCR_DUPLEX);
			fiber = 1;
			break;
		default:
			DBG(AFE_DPHY, "checking for 100BaseFX link");
			/* fiber is 100 Mb FDX */
			afe_miiwrite(afep, phyaddr, MII_REG_BMCR,
			    MII_BMCR_SPEED | MII_BMCR_DUPLEX);
			drv_usecwait(50);

			mcr = afe_miiread(afep, phyaddr, AFE_PHY_MCR);
			mcr |= AFE_MCR_FIBER;
			afe_miiwrite(afep, phyaddr, AFE_PHY_MCR, mcr);
			drv_usecwait(500);

			/* if fiber is active, use it */
			if ((afe_miiread(afep, phyaddr, MII_REG_BMSR) &
				MII_BMSR_LINK)) {
				bmcr = MII_BMCR_SPEED | MII_BMCR_DUPLEX;
				fiber = 1;
			} else {
				mcr &= ~AFE_MCR_FIBER;
				fiber = 0;
			}
			break;
		}
		afe_miiwrite(afep, phyaddr, AFE_PHY_MCR, mcr);
		drv_usecwait(500);
	}

	if (fiber) {
		/* fiber only supports 100FDX(?) */
		bmsr &= ~(MII_BMSR_100BT4 |
		    MII_BMSR_100HDX | MII_BMSR_10FDX | MII_BMSR_10HDX);
		bmsr |= MII_BMSR_100FDX;
	}

	/* disable modes not supported in hardware */
	validmode = 0;
	if (!(bmsr & MII_BMSR_100BT4)) {
		afep->afe_adv_100T4 = 0;
	} else {
		validmode = MII_ANEG_100BT4;
	}
	if (!(bmsr & MII_BMSR_100FDX)) {
		afep->afe_adv_100fdx = 0;
	} else {
		validmode = MII_ANEG_100FDX;
	}
	if (!(bmsr & MII_BMSR_100HDX)) {
		afep->afe_adv_100hdx = 0;
	} else {
		validmode = MII_ANEG_100HDX;
	}
	if (!(bmsr & MII_BMSR_10FDX)) {
		afep->afe_adv_10fdx = 0;
	} else {
		validmode = MII_ANEG_10FDX;
	}
	if (!(bmsr & MII_BMSR_10HDX)) {
		afep->afe_adv_10hdx = 0;
	} else {
		validmode = MII_ANEG_10HDX;
	}
	if (!(bmsr & MII_BMSR_ANA)) {
		afep->afe_adv_aneg = 0;
		force = 1;
	}

	cnt = 0;
	if (afep->afe_adv_100T4) {
		anar |= MII_ANEG_100BT4;
		cnt++;
	}
	if (afep->afe_adv_100fdx) {
		anar |= MII_ANEG_100FDX;
		cnt++;
	}
	if (afep->afe_adv_100hdx) {
		anar |= MII_ANEG_100HDX;
		cnt++;
	}
	if (afep->afe_adv_10fdx) {
		anar |= MII_ANEG_10FDX;
		cnt++;
	}
	if (afep->afe_adv_10hdx) {
		anar |= MII_ANEG_10HDX;
		cnt++;
	}

	/*
	 * Make certain at least one valid link mode is selected.
	 */
	if (!cnt) {
		char	*s;
		afe_error(afep->afe_dip, "No valid link mode selected.");
		switch (validmode) {
		case MII_ANEG_100BT4:
			s = "100 Base T4";
			afep->afe_adv_100T4 = 1;
			break;
		case MII_ANEG_100FDX:
			s = "100 Mbps Full-Duplex";
			afep->afe_adv_100fdx = 1;
			break;
		case MII_ANEG_100HDX:
			s = "100 Mbps Half-Duplex";
			afep->afe_adv_100hdx = 1;
			break;
		case MII_ANEG_10FDX:
			s = "10 Mbps Full-Duplex";
			afep->afe_adv_10fdx = 1;
			break;
		case MII_ANEG_10HDX:
			s = "10 Mbps Half-Duplex";
			afep->afe_adv_10hdx = 1;
			break;
		default:
			s = "unknown";
			break;
		}
		anar |= validmode;
		afe_error(afep->afe_dip, "Falling back to %s mode.", s);
	}

	if (fiber) {
		bmcr = MII_BMCR_SPEED | MII_BMCR_DUPLEX;
	} else if ((afep->afe_adv_aneg) && (bmsr & MII_BMSR_ANA)) {
		DBG(AFE_DPHY, "using autoneg mode");
		bmcr = (MII_BMCR_ANEG | MII_BMCR_RANEG);
	} else {
		DBG(AFE_DPHY, "using forced mode");
		force = 1;
		if (afep->afe_adv_100fdx) {
			bmcr = (MII_BMCR_SPEED | MII_BMCR_DUPLEX);
		} else if (afep->afe_adv_100hdx) {
			bmcr = MII_BMCR_SPEED;
		} else if (afep->afe_adv_10fdx) {
			bmcr = MII_BMCR_DUPLEX;
		} else {
			/* 10HDX */
			bmcr = 0;
		}
	}

	afep->afe_forcephy = force;

	DBG(AFE_DPHY, "programming anar to 0x%x", anar);
	afe_miiwrite(afep, phyaddr, MII_REG_ANAR, anar);
	DBG(AFE_DPHY, "programming bmcr to 0x%x", bmcr);
	afe_miiwrite(afep, phyaddr, MII_REG_BMCR, bmcr);

	if (nosqe) {
		uint16_t	pilr;
		/* 
		 * work around for errata 983B_0416 -- duplex light flashes
		 * in 10 HDX.  we just disable SQE testing on the device.
		 */
		pilr = afe_miiread(afep, phyaddr, AFE_PHY_PILR);
		pilr |= AFE_PILR_NOSQE;
		afe_miiwrite(afep, phyaddr, AFE_PHY_PILR, pilr);
	}

	/*
	 * schedule a query of the link status
	 */
	PUTCSR(afep, AFE_CSR_TIMER, AFE_TIMER_LOOP |
	    (AFE_LINKTIMER * 1000 / AFE_TIMER_USEC));
}

static void
afe_reportlink(afe_t *afep)
{
	int changed = 0;

	if (afep->afe_ifspeed != afep->afe_lastifspeed) {
		afep->afe_lastifspeed = afep->afe_ifspeed;
		changed++;
	}
	if (afep->afe_duplex != afep->afe_lastduplex) {
		afep->afe_lastduplex = afep->afe_duplex;
		changed++;
	}
	if (afep->afe_linkup && changed) {
		char	*media;
		switch (afep->afe_media) {
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
		cmn_err(CE_NOTE, afep->afe_ifspeed ?
		    "%s%d: %s %d Mbps %s-Duplex (%s) Link Up" :
		    "%s%d: Unknown %s MII Link Up",
		    ddi_driver_name(afep->afe_dip),
		    ddi_get_instance(afep->afe_dip),
		    afep->afe_forcephy ? "Forced" : "Auto-Negotiated",
		    (int)(afep->afe_ifspeed / 1000000),
		    (afep->afe_duplex == GLD_DUPLEX_FULL) ? "Full" : "Half",
		    media);
		afep->afe_lastlinkdown = 0;
	} else if (afep->afe_linkup) {
		afep->afe_lastlinkdown = 0;
	} else {
		DBG(AFE_DPHY, "no link");
		/* link lost, warn once every 10 seconds */
		if ((ddi_get_time() - afep->afe_lastlinkdown) > 10) {
			/* we've lost link, only warn on transition */
			afe_error(afep->afe_dip, "Link Down -- Cable Problem?");
			afep->afe_lastlinkdown = ddi_get_time();
		}
	}
}

static void
afe_checklink(afe_t *afep)
{
	if ((afep->afe_flags & AFE_RUNNING) == 0)
		return;
	switch (AFE_MODEL(afep)) {
	case AFE_MODEL_COMET:
		afe_checklinkcomet(afep);
		break;
	case AFE_MODEL_CENTAUR:
		afe_checklinkcentaur(afep);
		break;
	}
}

static void
afe_checklinkcomet(afe_t *afep)
{
	ushort		xciis;
	int		reinit = 0;

	xciis = GETCSR16(afep, AFE_CSR_XCIIS);
	if (xciis & AFE_XCIIS_PDF) {
		afe_error(afep->afe_dip, "Parallel detection fault detected!");
	}
	if (xciis & AFE_XCIIS_RF) {
		afe_error(afep->afe_dip, "Remote fault detected.");
	}
	if (xciis & AFE_XCIIS_LFAIL) {
		if (afep->afe_linkup) {
			reinit++;
		}
		afep->afe_ifspeed = 0;
		afep->afe_linkup = 0;
		afep->afe_duplex = GLD_DUPLEX_UNKNOWN;
		afe_reportlink(afep);
		if (reinit) {
			afe_phyinit(afep);
		}
		return;
	}

	afep->afe_linkup = 1;
	afep->afe_ifspeed = (xciis & AFE_XCIIS_SPEED) ? 100000000 : 10000000;
	if (xciis & AFE_XCIIS_DUPLEX) {
		afep->afe_duplex = GLD_DUPLEX_FULL;
	} else {
		afep->afe_duplex = GLD_DUPLEX_HALF;
	}

	afe_reportlink(afep);
}

static void
afe_checklinkcentaur(afe_t *afep)
{
	unsigned	opmode;
	int		reinit = 0;

	opmode = GETCSR(afep, AFE_CSR_OPM);
	if ((opmode & AFE_OPM_MODE) == AFE_OPM_MACONLY) {
		DBG(AFE_DPHY, "Centaur running in MAC-only mode");
		afe_checklinkmii(afep);
		return;
	}
	DBG(AFE_DPHY, "Centaur running in single chip mode");
	if ((opmode & AFE_OPM_LINK) == 0) {
		if (afep->afe_linkup) {
			reinit++;
		}
		afep->afe_ifspeed = 0;
		afep->afe_duplex = GLD_DUPLEX_UNKNOWN;
		afep->afe_linkup = 0;
		afep->afe_media = GLDM_UNKNOWN;
		afe_reportlink(afep);
		if (reinit) {
			afe_phyinit(afep);
		}
		return;
	}
	afep->afe_media = GLDM_TP;
	if ((afep->afe_flags & AFE_HASFIBER) &&
	    (afe_miiread(afep, afep->afe_phyaddr,
		AFE_PHY_MCR) & AFE_MCR_FIBER)) {
		afep->afe_media = GLDM_FIBER;
	}

	afep->afe_linkup = 1;
	afep->afe_ifspeed = (opmode & AFE_OPM_SPEED) ? 100000000 : 10000000;
	if (opmode & AFE_OPM_DUPLEX) {
		afep->afe_duplex = GLD_DUPLEX_FULL;
	} else {
		afep->afe_duplex = GLD_DUPLEX_HALF;
	}
	afe_reportlink(afep);
}

static void
afe_checklinkmii(afe_t *afep)
{
	/* read MII state registers */
	ushort 		bmsr;
	ushort 		bmcr;
	ushort 		anar;
	ushort 		anlpar;
	int		reinit = 0;

	/* read this twice, to clear latched link state */
	bmsr = afe_miiread(afep, afep->afe_phyaddr, MII_REG_BMSR);
	bmsr = afe_miiread(afep, afep->afe_phyaddr, MII_REG_BMSR);
	bmcr = afe_miiread(afep, afep->afe_phyaddr, MII_REG_BMCR);
	anar = afe_miiread(afep, afep->afe_phyaddr, MII_REG_ANAR);
	anlpar = afe_miiread(afep, afep->afe_phyaddr, MII_REG_ANLPAR);

	if (bmsr & MII_BMSR_RFAULT) {
		afe_error(afep->afe_dip, "Remote fault detected.");
	}
	if (bmsr & MII_BMSR_JABBER) {
		afe_error(afep->afe_dip, "Jabber condition detected.");
	}
	if ((bmsr & MII_BMSR_LINK) == 0) {
		/* no link */
		if (afep->afe_linkup) {
			reinit = 1;
		}
		afep->afe_ifspeed = 0;
		afep->afe_duplex = GLD_DUPLEX_UNKNOWN;
		afep->afe_linkup = 0;
		afep->afe_media = GLDM_UNKNOWN;
		afe_reportlink(afep);
		if (reinit) {
			afe_phyinit(afep);
		}
		return;
	}

	afep->afe_media = GLDM_PHYMII;
	DBG(AFE_DCHATTY, "link up!");
	afep->afe_lastlinkdown = 0;
	afep->afe_linkup = 1;

	if (!(bmcr & MII_BMCR_ANEG)) {
		/* forced mode */
		if (bmcr & MII_BMCR_SPEED) {
			afep->afe_ifspeed = 100000000;
		} else {
			afep->afe_ifspeed = 10000000;
		}
		if (bmcr & MII_BMCR_DUPLEX) {
			afep->afe_duplex = GLD_DUPLEX_FULL;
		} else {
			afep->afe_duplex = GLD_DUPLEX_HALF;
		}
	} else if ((!(bmsr & MII_BMSR_ANA)) || (!(bmsr & MII_BMSR_ANC))) {
		afep->afe_ifspeed = 0;
		afep->afe_duplex = GLD_DUPLEX_UNKNOWN;
	} else if (anar & anlpar & MII_ANEG_100BT4) {
		afep->afe_ifspeed = 100000000;
		afep->afe_duplex = GLD_DUPLEX_HALF;
	} else if (anar & anlpar & MII_ANEG_100FDX) {
		afep->afe_ifspeed = 100000000;
		afep->afe_duplex = GLD_DUPLEX_FULL;
	} else if (anar & anlpar & MII_ANEG_100HDX) {
		afep->afe_ifspeed = 100000000;
		afep->afe_duplex = GLD_DUPLEX_HALF;
	} else if (anar & anlpar & MII_ANEG_10FDX) {
		afep->afe_ifspeed = 10000000;
		afep->afe_duplex = GLD_DUPLEX_FULL;
	} else if (anar & anlpar & MII_ANEG_10HDX) {
		afep->afe_ifspeed = 10000000;
		afep->afe_duplex = GLD_DUPLEX_HALF;
	} else {
		afep->afe_ifspeed = 0;
		afep->afe_duplex = GLD_DUPLEX_UNKNOWN;
	}

	afe_reportlink(afep);
}

static void
afe_miitristate(afe_t *afep)
{
	unsigned val = AFE_SROM_WRITE | AFE_MII_CONTROL;
	PUTCSR(afep, AFE_CSR_SPR, val);
	drv_usecwait(1);
	PUTCSR(afep, AFE_CSR_SPR, val | AFE_MII_CLOCK);
	drv_usecwait(1);
}

static void
afe_miiwritebit(afe_t *afep, int bit)
{
	unsigned val = bit ? AFE_MII_DOUT : 0;
	PUTCSR(afep, AFE_CSR_SPR, val);
	drv_usecwait(1);
	PUTCSR(afep, AFE_CSR_SPR, val | AFE_MII_CLOCK);
	drv_usecwait(1);
}

static int
afe_miireadbit(afe_t *afep)
{
	unsigned val = AFE_MII_CONTROL | AFE_SROM_READ;
	int bit;
	PUTCSR(afep, AFE_CSR_SPR, val);
	drv_usecwait(1);
	bit = (GETCSR(afep, AFE_CSR_SPR) & AFE_MII_DIN) ? 1 : 0;
	PUTCSR(afep, AFE_CSR_SPR, val | AFE_MII_CLOCK);
	drv_usecwait(1);
	return (bit);
}

static unsigned
afe_miiread(afe_t *afep, int phy, int reg)
{
	/*
	 * ADMtek bugs ignore address decode bits -- they only
	 * support PHY at 1.
	 */
	if (phy != 1) {
		return (0xffff);
	}
	switch (AFE_MODEL(afep)) {
	case AFE_MODEL_COMET:
		return (afe_miireadcomet(afep, phy, reg));
	case AFE_MODEL_CENTAUR:
		return (afe_miireadgeneral(afep, phy, reg));
	}
	return (0xffff);
}

static unsigned
afe_miireadgeneral(afe_t *afep, int phy, int reg)
{
	unsigned	value = 0;
	int		i;

	/* send the 32 bit preamble */
	for (i = 0; i < 32; i++) {
		afe_miiwritebit(afep, 1);
	}

	/* send the start code - 01b */
	afe_miiwritebit(afep, 0);
	afe_miiwritebit(afep, 1);

	/* send the opcode for read, - 10b */
	afe_miiwritebit(afep, 1);
	afe_miiwritebit(afep, 0);

	/* next we send the 5 bit phy address */
	for (i = 0x10; i > 0; i >>= 1) {
		afe_miiwritebit(afep, (phy & i) ? 1 : 0);
	}

	/* the 5 bit register address goes next */
	for (i = 0x10; i > 0; i >>= 1) {
		afe_miiwritebit(afep, (reg & i) ? 1 : 0);
	}

	/* turnaround - tristate followed by logic 0 */
	afe_miitristate(afep);
	afe_miiwritebit(afep, 0);

	/* read the 16 bit register value */
	for (i = 0x8000; i > 0; i >>= 1) {
		value <<= 1;
		value |= afe_miireadbit(afep);
	}
	afe_miitristate(afep);
	return (value);
}

static unsigned
afe_miireadcomet(afe_t *afep, int phy, int reg)
{
	if (phy != 1) {
		return (0xffff);
	}
	switch (reg) {
	case MII_REG_BMCR:
		reg = AFE_CSR_BMCR;
		break;
	case MII_REG_BMSR:
		reg = AFE_CSR_BMSR;
		break;
	case MII_REG_PHYIDR1:
		reg = AFE_CSR_PHYIDR1;
		break;
	case MII_REG_PHYIDR2:
		reg = AFE_CSR_PHYIDR2;
		break;
	case MII_REG_ANAR:
		reg = AFE_CSR_ANAR;
		break;
	case MII_REG_ANLPAR:
		reg = AFE_CSR_ANLPAR;
		break;
	case MII_REG_ANER:
		reg = AFE_CSR_ANER;
		break;
	default:
		return (0);
	}
	return (GETCSR16(afep, reg) & 0xFFFF);
}

static void
afe_miiwrite(afe_t *afep, int phy, int reg, ushort val)
{
	/*
	 * ADMtek bugs ignore address decode bits -- they only
	 * support PHY at 1.
	 */
	if (phy != 1) {
		return;
	}
	switch (AFE_MODEL(afep)) {
	case AFE_MODEL_COMET:
		afe_miiwritecomet(afep, phy, reg, val);
		break;
	case AFE_MODEL_CENTAUR:
		afe_miiwritegeneral(afep, phy, reg, val);
		break;
	}
}

static void
afe_miiwritegeneral(afe_t *afep, int phy, int reg, ushort val)
{
	int i;

	/* send the 32 bit preamble */
	for (i = 0; i < 32; i++) {
		afe_miiwritebit(afep, 1);
	}

	/* send the start code - 01b */
	afe_miiwritebit(afep, 0);
	afe_miiwritebit(afep, 1);

	/* send the opcode for write, - 01b */
	afe_miiwritebit(afep, 0);
	afe_miiwritebit(afep, 1);

	/* next we send the 5 bit phy address */
	for (i = 0x10; i > 0; i >>= 1) {
		afe_miiwritebit(afep, (phy & i) ? 1 : 0);
	}

	/* the 5 bit register address goes next */
	for (i = 0x10; i > 0; i >>= 1) {
		afe_miiwritebit(afep, (reg & i) ? 1 : 0);
	}

	/* turnaround - tristate followed by logic 0 */
	afe_miitristate(afep);
	afe_miiwritebit(afep, 0);

	/* now write out our data (16 bits) */
	for (i = 0x8000; i > 0; i >>= 1) {
		afe_miiwritebit(afep, (val & i) ? 1 : 0);
	}

	/* idle mode */
	afe_miitristate(afep);
}

static void
afe_miiwritecomet(afe_t *afep, int phy, int reg, ushort val)
{
	if (phy != 1) {
		return;
	}
	switch (reg) {
	case MII_REG_BMCR:
		reg = AFE_CSR_BMCR;
		break;
	case MII_REG_BMSR:
		reg = AFE_CSR_BMSR;
		break;
	case MII_REG_PHYIDR1:
		reg = AFE_CSR_PHYIDR1;
		break;
	case MII_REG_PHYIDR2:
		reg = AFE_CSR_PHYIDR2;
		break;
	case MII_REG_ANAR:
		reg = AFE_CSR_ANAR;
		break;
	case MII_REG_ANLPAR:
		reg = AFE_CSR_ANLPAR;
		break;
	case MII_REG_ANER:
		reg = AFE_CSR_ANER;
		break;
	default:
		return;
	}
	PUTCSR16(afep, reg, val);
}

/*
 * Multicast support.
 */
/*
 * Calculates the CRC of the multicast address, the lower 6 bits of
 * which are used to set up the multicast filter.
 */
static unsigned
afe_etherhashbe(uchar_t *addrp)
{
	unsigned	hash = 0xffffffffU;
	unsigned	carry;
	int		byte;
	int		bit;
	uchar_t		curr;
	static unsigned	poly = 0x04c11db6U;

	for (byte = 0; byte < ETHERADDRL; byte++) {
		curr = addrp[byte];
		for (bit = 0; bit < 8; bit++, curr >>= 1) {
			carry = ((hash & 0x80000000U) ? 1 : 0) ^ (curr & 0x1);
			hash <<= 1;
			curr >>= 1;
			if (carry) {
				hash = (hash ^ poly) | carry;
			}
		}
	}
	return (hash);
}

static int
afe_start(gld_mac_info_t *macinfo)
{
	afe_t	*afep = (afe_t *)macinfo->gldm_private;

	/* grab exclusive access to the card */
	mutex_enter(&afep->afe_intrlock);
	mutex_enter(&afep->afe_xmtlock);

	if (afe_startmac(afep) == GLD_SUCCESS) {
		afep->afe_flags |= AFE_RUNNING;
	}

	mutex_exit(&afep->afe_xmtlock);
	mutex_exit(&afep->afe_intrlock);
	return (GLD_SUCCESS);
}

static int
afe_stop(gld_mac_info_t *macinfo)
{
	afe_t	*afep = (afe_t *)macinfo->gldm_private;

	/* exclusive access to the hardware! */
	mutex_enter(&afep->afe_intrlock);
	mutex_enter(&afep->afe_xmtlock);

	afe_stopmac(afep);

	afep->afe_flags &= ~AFE_RUNNING;
	mutex_exit(&afep->afe_xmtlock);
	mutex_exit(&afep->afe_intrlock);
	return (GLD_SUCCESS);
}

static int
afe_startmac(afe_t *afep)
{
	int		i;
	uint32_t	thresh;

	/* FIXME: do the power management thing */

	/* verify exclusive access to the card */
	ASSERT(mutex_owned(&afep->afe_intrlock));
	ASSERT(mutex_owned(&afep->afe_xmtlock));

	/* stop the card */
	CLRBIT(afep, AFE_CSR_NAR, AFE_TX_ENABLE | AFE_RX_ENABLE);

	/* free any pending buffers */
	for (i = 0; i < AFE_TXRING; i++) {
		if (afep->afe_txbufs[i]) {
			afe_freebuf(afep->afe_txbufs[i]);
			afep->afe_txbufs[i] = NULL;
		}
	}
	for (i = 0; i < AFE_RXRING; i++) {
		if (afep->afe_rxbufs[i]) {
			afe_freebuf(afep->afe_rxbufs[i]);
			afep->afe_rxbufs[i] = NULL;
		}
	}

	/* reset the descriptor ring pointers */
	afep->afe_rxcurrent = 0;
	afep->afe_txreclaim = 0;
	afep->afe_txsend = 0;
	afep->afe_txavail = AFE_TXRING;
	/* point hardware at the descriptor rings */
	PUTCSR(afep, AFE_CSR_TDB, afep->afe_desc_txpaddr);
	PUTCSR(afep, AFE_CSR_RDB, afep->afe_desc_rxpaddr);

	/*
	 * We only do this if we are initiating a hard reset
	 * of the chip, typically at start of day.  When we're
	 * just setting promiscuous mode or somesuch, this becomes
	 * wasteful.
	 */
	DBG(AFE_DCHATTY, "phy reset");

	afe_phyinit(afep);

	/* set up transmit descriptor ring */
	for (i = 0; i < AFE_TXRING; i++) {
		afe_desc_t	*tmdp = &afep->afe_txdescp[i];
		unsigned	control = 0;
		if (i == (AFE_TXRING - 1)) {
			control |= AFE_TXCTL_ENDRING;
		}
		PUTDESC(afep, tmdp->desc_status, 0);
		PUTDESC(afep, tmdp->desc_control, control);
		PUTDESC(afep, tmdp->desc_buffer1, 0);
		PUTDESC(afep, tmdp->desc_buffer2, 0);
		SYNCDESC(afep, tmdp, DDI_DMA_SYNC_FORDEV);
	}

	/* make the receive buffers available */
	for (i = 0; i < AFE_RXRING; i++) {
		afe_buf_t	*bufp;
		afe_desc_t	*rmdp = &afep->afe_rxdescp[i];
		unsigned	control;

		if ((bufp = afe_getbuf(afep, 1)) == NULL) {
			/* this should never happen! */
			afe_error(afep->afe_dip, "out of buffers!");
			return (GLD_FAILURE);
		}
		afep->afe_rxbufs[i] = bufp;

		control = AFE_BUFSZ & AFE_RXCTL_BUFLEN1;
		if (i == (AFE_RXRING - 1)) {
			control |= AFE_RXCTL_ENDRING;
		}
		PUTDESC(afep, rmdp->desc_buffer1, bufp->bp_paddr);
		PUTDESC(afep, rmdp->desc_buffer2, 0);
		PUTDESC(afep, rmdp->desc_control, control);
		PUTDESC(afep, rmdp->desc_status, AFE_RXSTAT_OWN);

		/* sync the descriptor for the device */
		SYNCDESC(afep, rmdp, DDI_DMA_SYNC_FORDEV);
	}

	DBG(AFE_DCHATTY, "descriptors setup");

	/* clear the lost packet counter (cleared on read) */
	(void) GETCSR(afep, AFE_CSR_LPC);

	/* program tx threshold bits */
	CLRBIT(afep, AFE_CSR_NAR, AFE_NAR_TR | AFE_NAR_SF);
	SETBIT(afep, AFE_CSR_NAR, afe_txthresh[afep->afe_txthresh]);
	/* disable SQE test */
	SETBIT(afep, AFE_CSR_NAR, AFE_NAR_HBD);

	/* enable interrupts */
	afe_enableinterrupts(afep);

	/* start the card */
	SETBIT(afep, AFE_CSR_NAR, AFE_TX_ENABLE | AFE_RX_ENABLE);

	return (GLD_SUCCESS);
}

static int
afe_stopmac(afe_t *afep)
{
	/* exclusive access to the hardware! */
	ASSERT(mutex_owned(&afep->afe_intrlock));
	ASSERT(mutex_owned(&afep->afe_xmtlock));

	/* FIXME: possibly wait for transmits to drain */

	/* stop the card */
	CLRBIT(afep, AFE_CSR_NAR, AFE_TX_ENABLE | AFE_RX_ENABLE);

	/* stop the on-card timer */
	PUTCSR(afep, AFE_CSR_TIMER, 0);

	/* disable interrupts */
	afe_disableinterrupts(afep);

	afep->afe_linkup = 0;
	afep->afe_ifspeed = 0;
	afep->afe_duplex = GLD_DUPLEX_UNKNOWN;

	return (GLD_SUCCESS);
}

/*
 * Allocate descriptors.
 */
static int
afe_allocrings(afe_t *afep)
{
	int			size;
	int			rval;
	int			i;
	size_t			real_len;
	ddi_dma_cookie_t	cookie;
	uint			ncookies;
	ddi_dma_handle_t	dmah;
	ddi_acc_handle_t	acch;

	afep->afe_desc_dmahandle = NULL;
	afep->afe_desc_acchandle = NULL;

	size = (AFE_RXRING + AFE_TXRING) * sizeof (afe_desc_t);
	rval = ddi_dma_alloc_handle(afep->afe_dip, &afe_dma_attr,
	    DDI_DMA_SLEEP, 0, &dmah);
	if (rval != DDI_SUCCESS) {
		afe_error(afep->afe_dip, "unable to allocate DMA handle for "
		    "media descriptors, rval = %d", rval);
		return (DDI_FAILURE);
	}

	rval = ddi_dma_mem_alloc(dmah, size, &afe_devattr,
	    DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0, &afep->afe_desc_kaddr,
	    &real_len,  &acch);
	if (rval != DDI_SUCCESS) {
		afe_error(afep->afe_dip, "unable to allocate DMA memory for "
		    "media descriptors");
		ddi_dma_free_handle(&dmah);
		return (DDI_FAILURE);
	}

	rval = ddi_dma_addr_bind_handle(dmah, NULL, afep->afe_desc_kaddr,
	    size, DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, 0, &cookie, &ncookies);
	if (rval != DDI_DMA_MAPPED) {
		afe_error(afep->afe_dip, "unable to bind DMA handle for "
		    "media descriptors");
		ddi_dma_mem_free(&acch);
		ddi_dma_free_handle(&dmah);
		return (DDI_FAILURE);
	}

	/* we take the 32-bit physical address out of the cookie */
	afep->afe_desc_rxpaddr = cookie.dmac_address;
	afep->afe_desc_txpaddr = cookie.dmac_address +
	    (sizeof (afe_desc_t) * AFE_RXRING);

	DBG(AFE_DDMA, "rx phys addr = 0x%x", afep->afe_desc_rxpaddr);
	DBG(AFE_DDMA, "tx phys addr = 0x%x", afep->afe_desc_txpaddr);

	if (ncookies != 1) {
		afe_error(afep->afe_dip, "too many DMA cookies for media "
		    "descriptors");
		(void) ddi_dma_unbind_handle(afep->afe_desc_dmahandle);
		ddi_dma_mem_free(&acch);
		ddi_dma_free_handle(&dmah);
		return (DDI_FAILURE);
	}

	size = sizeof (afe_buf_t *);

	/* allocate buffer pointers (not the buffers themselves, yet) */
	afep->afe_buftab = kmem_zalloc(afep->afe_numbufs * size, KM_SLEEP);
	afep->afe_txbufs = kmem_zalloc(AFE_TXRING * size, KM_SLEEP);
	afep->afe_rxbufs = kmem_zalloc(AFE_RXRING * size, KM_SLEEP);

	/* save off the descriptor handles */
	afep->afe_desc_dmahandle = dmah;
	afep->afe_desc_acchandle = acch;

	/* now allocate the actual buffers */
	for (i = 0; i < afep->afe_numbufs; i++) {
		afe_buf_t		*bufp;

		dmah = NULL;
		acch = NULL;
		bufp = kmem_zalloc(sizeof (afe_buf_t), KM_SLEEP);

		if (ddi_dma_alloc_handle(afep->afe_dip, &afe_dma_attr,
		    DDI_DMA_SLEEP, NULL, &dmah) != DDI_SUCCESS) {
			kmem_free(bufp, sizeof (afe_buf_t));
			return (DDI_FAILURE);
		}
		if (ddi_dma_mem_alloc(dmah, AFE_BUFSZ, &afe_bufattr,
		    DDI_DMA_STREAMING, DDI_DMA_SLEEP, NULL,
		    &bufp->bp_buf, &real_len, &acch) != DDI_SUCCESS) {
			ddi_dma_free_handle(&dmah);
			kmem_free(bufp, sizeof (afe_buf_t));
			return (DDI_FAILURE);
		}
		if (ddi_dma_addr_bind_handle(dmah, NULL, bufp->bp_buf,
		    real_len, DDI_DMA_STREAMING | DDI_DMA_RDWR,
		    DDI_DMA_SLEEP, 0, &cookie, &ncookies) != DDI_DMA_MAPPED) {
			ddi_dma_mem_free(&acch);
			ddi_dma_free_handle(&dmah);
			kmem_free(bufp, sizeof (afe_buf_t));
			return (DDI_FAILURE);
		}

		bufp->bp_afep = afep;
		bufp->bp_frtn.free_func =  afe_freebuf;
		bufp->bp_frtn.free_arg = (caddr_t)bufp;
		bufp->bp_dma_handle = dmah;
		bufp->bp_acc_handle = acch;
		bufp->bp_paddr = cookie.dmac_address;
		DBG(AFE_DDMA, "buf #%d phys addr = 0x%x", i,
		    bufp->bp_paddr);

		/* stick it in the stack */
		afep->afe_buftab[i] = bufp;
	}

	/* set the top of the stack */
	afep->afe_topbuf = afep->afe_numbufs;

	/* descriptor pointers */
	afep->afe_rxdescp = (afe_desc_t *)afep->afe_desc_kaddr;
	afep->afe_txdescp = afep->afe_rxdescp + AFE_RXRING;

	afep->afe_rxcurrent = 0;
	afep->afe_txreclaim = 0;
	afep->afe_txsend = 0;
	afep->afe_txavail = AFE_TXRING;
	return (DDI_SUCCESS);
}

static void
afe_freerings(afe_t *afep)
{
	int			i;
	afe_buf_t		*bufp;
	ddi_dma_handle_t	dmah;
	ddi_acc_handle_t	acch;

	for (i = 0; i < afep->afe_numbufs; i++) {
		bufp = afep->afe_buftab[i];
		if (bufp != NULL) {
			dmah = bufp->bp_dma_handle;
			acch = bufp->bp_acc_handle;

			if (ddi_dma_unbind_handle(dmah) == DDI_SUCCESS) {
				ddi_dma_mem_free(&acch);
				ddi_dma_free_handle(&dmah);
			} else {
				afe_error(afep->afe_dip,
				    "ddi_dma_unbind_handle failed!");
			}
			kmem_free(bufp, sizeof (afe_buf_t));
		}
	}

	DBG(AFE_DCHATTY, "freeing buffer pools");
	if (afep->afe_buftab) {
		kmem_free(afep->afe_buftab,
		    afep->afe_numbufs * sizeof (afe_buf_t *));
		afep->afe_buftab = NULL;
	}
	if (afep->afe_txbufs) {
		kmem_free(afep->afe_txbufs, AFE_TXRING * sizeof (afe_buf_t *));
		afep->afe_txbufs = NULL;
	}
	if (afep->afe_rxbufs) {
		kmem_free(afep->afe_rxbufs, AFE_RXRING * sizeof (afe_buf_t *));
		afep->afe_rxbufs = NULL;
	}

	DBG(AFE_DCHATTY, "free descriptor DMA resources");
	(void) ddi_dma_unbind_handle(afep->afe_desc_dmahandle);
	ddi_dma_mem_free(&afep->afe_desc_acchandle);
	ddi_dma_free_handle(&afep->afe_desc_dmahandle);
}

/*
 * Buffer management.
 */
static afe_buf_t *
afe_getbuf(afe_t *afep, int pri)
{
	int		top;
	afe_buf_t	*bufp;

	mutex_enter(&afep->afe_buflock);
	top = afep->afe_topbuf;
	if ((top == 0) || ((pri == 0) && (top < AFE_RSVDBUFS))) {
		mutex_exit(&afep->afe_buflock);
		return (NULL);
	}
	top--;
	bufp = afep->afe_buftab[top];
	afep->afe_buftab[top] = NULL;
	afep->afe_topbuf = top;
	mutex_exit(&afep->afe_buflock);
	return (bufp);
}

static void
afe_freebuf(afe_buf_t *bufp)
{
	afe_t *afep = bufp->bp_afep;
	mutex_enter(&afep->afe_buflock);
	afep->afe_buftab[afep->afe_topbuf++] = bufp;
	bufp->bp_flags = 0;
	mutex_exit(&afep->afe_buflock);
}

/*
 * Interrupt service routine.
 */
static unsigned
afe_intr(gld_mac_info_t *macinfo)
{
	afe_t		*afep = (afe_t *)macinfo->gldm_private;
	unsigned	status = 0;
	dev_info_t	*dip = afep->afe_dip;
	int		reset = 0;
	int		linkcheck = 0;
	int		wantw = 0;
	mblk_t		*mp = NULL, **mpp;

	afep->afe_intr++;

	mpp = &mp;

	mutex_enter(&afep->afe_intrlock);

	if (afep->afe_flags & AFE_SUSPENDED) {
		/* we cannot receive interrupts! */
		mutex_exit(&afep->afe_intrlock);
		return (DDI_INTR_UNCLAIMED);
	}


	/* get interrupt status bits */
	status = GETCSR(afep, AFE_CSR_SR2);
	if (!(status & AFE_INT_ALL)) {
		if (afep->afe_intrstat)
			KIOIP->intrs[KSTAT_INTR_SPURIOUS]++;
		mutex_exit(&afep->afe_intrlock);
		return (DDI_INTR_UNCLAIMED);
	}
	/* ack the interrupt */
	PUTCSR(afep, AFE_CSR_SR2, status & AFE_INT_ALL);
	if (afep->afe_intrstat)
		KIOIP->intrs[KSTAT_INTR_HARD]++;

	DBG(AFE_DINTR, "interrupted, status = %x", status);

	if (!(afep->afe_flags & AFE_RUNNING)) {
		/* not running, don't touch anything */
		mutex_exit(&afep->afe_intrlock);
		return (DDI_INTR_UNCLAIMED);
	}

	if (status & AFE_INT_LINKCHG) {
		/* rescan the link */
		linkcheck++;
		DBG(AFE_DINTR, "link change interrupt!");
	}

	if (status & (AFE_INT_TXOK|AFE_INT_TXJABBER|AFE_INT_TXUNDERFLOW)) {
		/* transmit completed */
		wantw = 1;
	}

	if (status & (AFE_INT_RXOK | AFE_INT_RXNOBUF)) {
		for (;;) {
			unsigned	status;
			afe_desc_t	*rmd =
			    &afep->afe_rxdescp[afep->afe_rxcurrent];

			/* sync it before we look at it */
			SYNCDESC(afep, rmd, DDI_DMA_SYNC_FORCPU);

			status = GETDESC(afep, rmd->desc_status);
			if (status & AFE_RXSTAT_OWN) {
				/* chip is still chewing on it */
				break;
			}

			DBG(AFE_DCHATTY, "reading packet at %d "
			    "(status = 0x%x, length=%d)",
			    afep->afe_rxcurrent, status, AFE_RXLENGTH(status));

			*mpp = afe_read(afep, afep->afe_rxcurrent, status);
			if (*mpp) {
				mpp = &(*mpp)->b_next;
			}

			/* give it back to the hardware */
			PUTDESC(afep, rmd->desc_status, AFE_RXSTAT_OWN);
			SYNCDESC(afep, rmd, DDI_DMA_SYNC_FORDEV);

			/* advance to next RMD */
			afep->afe_rxcurrent++;
			afep->afe_rxcurrent %= AFE_RXRING;

			/* poll demand the receiver */
			PUTCSR(afep, AFE_CSR_RDR, 1);
		}

		/* we rec'd a packet, if linkdown, verify */
		if (afep->afe_linkup == 0) {
			linkcheck++;
		}
		DBG(AFE_DCHATTY, "done receiving packets");
	}

	if (status & AFE_INT_RXNOBUF) {
		afep->afe_norcvbuf++;

		DBG(AFE_DINTR, "rxnobuf interrupt!");
	}

	if (status & (AFE_INT_RXNOBUF | AFE_INT_RXIDLE)) {
		/* restart the receiver */
		SETBIT(afep, AFE_CSR_NAR, AFE_RX_ENABLE);
	}

	if (status & AFE_INT_BUSERR) {
		reset = 1;
		switch (status & AFE_BERR_TYPE) {
		case AFE_BERR_PARITY:
			afe_error(dip, "PCI parity error detected");
			break;
		case AFE_BERR_TARGET_ABORT:
			afe_error(dip, "PCI target abort detected");
			break;
		case AFE_BERR_MASTER_ABORT:
			afe_error(dip, "PCI master abort detected");
			break;
		default:
			afe_error(dip, "Unknown PCI bus error");
			break;
		}
	}
	if (status & AFE_INT_TXUNDERFLOW) {
		afe_error(dip, "TX underflow detected, adjusting");
		if (afep->afe_txthresh < AFE_MAX_TXTHRESH)
			afep->afe_txthresh++;
		reset = 1;
	}
	if (status & (AFE_INT_TXJABBER)) {
		afe_error(dip, "TX jabber detected");
		reset = 1;
	}

	if (status & AFE_INT_RXJABBER) {
		afe_error(dip, "RX jabber detected");
		afep->afe_errrcv++;
		reset = 1;
	}

	/*
	 * Update the missed frame count.
	 */
	afep->afe_missed += (GETCSR(afep, AFE_CSR_LPC) & AFE_LPC_COUNT);

	mutex_exit(&afep->afe_intrlock);

	/*
	 * Send up packets.  We do this outside of the intrlock.
	 */
	while (mp) {
		mblk_t *nmp = mp->b_next;
		mp->b_next = NULL;
		gld_recv(macinfo, mp);
		mp = nmp;
	}
	
	/*
	 * Reclaim transmitted buffers and reschedule any waiters.
	 */
	if (wantw) {
		mutex_enter(&afep->afe_xmtlock);
		afe_reclaim(afep);
		mutex_exit(&afep->afe_xmtlock);
		gld_sched(macinfo);
	}
	
	if (linkcheck) {
		mutex_enter(&afep->afe_xmtlock);
		afe_checklink(afep);
		mutex_exit(&afep->afe_xmtlock);
	}

	if (reset) {
		/* reset the chip in an attempt to fix things */
		mutex_enter(&afep->afe_intrlock);
		mutex_enter(&afep->afe_xmtlock);
		/*
		 * we only reset the chip if we think it should be running
		 * This test is necessary to close a race with gld_stop.
		 */
		if (afep->afe_flags & AFE_RUNNING) {
			afe_stopmac(afep);
			afe_resetmac(afep);
			afe_startmac(afep);
		}
		mutex_exit(&afep->afe_xmtlock);
		mutex_exit(&afep->afe_intrlock);
	}
	return (DDI_INTR_CLAIMED);
}

static void
afe_enableinterrupts(afe_t *afep)
{
	PUTCSR(afep, AFE_CSR_IER2, AFE_INT_WANTED);
	/* enable immediate link change notification on Centaur */
	if (AFE_MODEL(afep) == AFE_MODEL_COMET) {
		/*
		 * On the Comet, this is the internal transceiver
		 * interrupt.  We program the Comet's built-in PHY to
		 * enable certain interrupts.
		 */
		PUTCSR16(afep, AFE_CSR_XIE, AFE_XIE_LDE | AFE_XIE_ANCE);
	}
}

static void
afe_disableinterrupts(afe_t *afep)
{
	/* disable further interrupts */
	PUTCSR(afep, AFE_CSR_IER2, AFE_INT_NONE);

	/* admtek variants thru special registers */
	PUTCSR(afep, AFE_CSR_SR2, AFE_INT_ALL);
}

static int
afe_send(gld_mac_info_t *macinfo, mblk_t *mp)
{
	afe_t			*afep = (afe_t *)macinfo->gldm_private;
	int			len;
	afe_buf_t		*bufp;
	afe_desc_t		*tmdp;
	unsigned		control;

	len = afe_msgsize(mp);
	if (len > ETHERMAX) {
		afep->afe_errxmt++;
		DBG(AFE_DWARN, "output packet too long!");
		return (GLD_BADARG);
	}

	/* grab a transmit buffer */
	if ((bufp = afe_getbuf(afep, 1)) == NULL) {
		DBG(AFE_DXMIT, "no buffer available");
		return (GLD_NORESOURCES);
	}

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

	DBG(AFE_DCHATTY, "syncing mp = 0x%p,  buf 0x%p, len %d", mp,
	    bufp->bp_buf, len);

	/* sync the buffer for the device */
	SYNCBUF(bufp, len, DDI_DMA_SYNC_FORDEV);

	/* acquire transmit lock */
	mutex_enter(&afep->afe_xmtlock);

	/* if the interface is suspended, put it back on the queue */
	if (afep->afe_flags & AFE_SUSPENDED) {
		DBG(AFE_DWARN, "interface suspended");
		mutex_exit(&afep->afe_xmtlock);
		afe_freebuf(bufp);
		return (GLD_NORESOURCES);
	}

	if (afep->afe_txavail == 0) {
		/* no more tmds */
		mutex_exit(&afep->afe_xmtlock);
		afe_freebuf(bufp);
		DBG(AFE_DXMIT, "out of tmds");
		return (GLD_NORESOURCES);
	}

	freemsg(mp);
	mp = NULL;

	afep->afe_txavail--;
	tmdp = &afep->afe_txdescp[afep->afe_txsend];
	control = AFE_TXCTL_FIRST | AFE_TXCTL_LAST |
	    (AFE_TXCTL_BUFLEN1 & len) | AFE_TXCTL_INTCMPLTE;
	if (afep->afe_txsend == (AFE_TXRING - 1)) {
		control |= AFE_TXCTL_ENDRING;
	}
	PUTDESC(afep, tmdp->desc_control, control);
	PUTDESC(afep, tmdp->desc_buffer1, bufp->bp_paddr);
	PUTDESC(afep, tmdp->desc_buffer2, 0);
	PUTDESC(afep, tmdp->desc_status, AFE_TXSTAT_OWN);

	afep->afe_txbufs[afep->afe_txsend] = bufp;

	/* sync the descriptor out to the device */
	SYNCDESC(afep, tmdp, DDI_DMA_SYNC_FORDEV);

	/* update the ring pointer */
	afep->afe_txsend++;
	afep->afe_txsend %= AFE_TXRING;

	/* wake up the chip */
	PUTCSR(afep, AFE_CSR_TDR, 0xffffffffU);

	mutex_exit(&afep->afe_xmtlock);

	return (GLD_SUCCESS);
}

/*
 * Reclaim buffers that have completed transmission.
 */
static void
afe_reclaim(afe_t *afep)
{
	afe_desc_t	*tmdp;

	if ((afep->afe_flags & AFE_RUNNING) == 0)
		return;

	for (;;) {
		unsigned	status;
		afe_buf_t	*bufp;

		tmdp = &afep->afe_txdescp[afep->afe_txreclaim];
		bufp = afep->afe_txbufs[afep->afe_txreclaim];
		if (bufp == NULL) {
			break;
		}

		/* sync it before we read it */
		SYNCDESC(afep, tmdp, DDI_DMA_SYNC_FORCPU);

		status = GETDESC(afep, tmdp->desc_status);

		if (status & AFE_TXSTAT_OWN) {
			/* chip is still working on it, we're done */
			break;
		}

		/* update statistics */
		if (status & AFE_TXSTAT_TXERR) {
			afep->afe_errxmt++;
		}

		if (status & AFE_TXSTAT_JABBER) {
			/* transmit jabber timeout */
			DBG(AFE_DXMIT, "tx jabber!");
			afep->afe_macxmt_errors++;
		}
		if (status & (AFE_TXSTAT_CARRLOST | AFE_TXSTAT_NOCARR)) {
			DBG(AFE_DXMIT, "no carrier!");
			afe_checklink(afep);
			afep->afe_carrier_errors++;
		}

		if (status & AFE_TXSTAT_UFLOW) {
			DBG(AFE_DXMIT, "tx underflow!");
			afep->afe_underflow++;
			afep->afe_macxmt_errors++;
		}

		/* only count SQE errors if the test is not disabled */
		if ((status & AFE_TXSTAT_SQE) &&
		    ((GETCSR(afep, AFE_CSR_NAR) & AFE_NAR_HBD) == 0)) {
			afep->afe_sqe_errors++;
			afep->afe_macxmt_errors++;
		}

		if (status & AFE_TXSTAT_DEFER) {
			afep->afe_defer_xmts++;
		}

		/* collision counting */
		if (status & AFE_TXSTAT_LATECOL) {
			DBG(AFE_DXMIT, "tx late collision!");
			afep->afe_tx_late_collisions++;
			afep->afe_collisions++;
		} else if (status & AFE_TXSTAT_EXCOLL) {
			DBG(AFE_DXMIT, "tx excessive collisions!");
			afep->afe_ex_collisions++;
			afep->afe_collisions += 16;
		} else if (AFE_TXCOLLCNT(status) == 1) {
			afep->afe_collisions++;
			afep->afe_first_collisions++;
		} else if (AFE_TXCOLLCNT(status)) {
			afep->afe_collisions += AFE_TXCOLLCNT(status);
			afep->afe_multi_collisions += AFE_TXCOLLCNT(status);
		}

		/* release the buffer */
		afe_freebuf(bufp);
		afep->afe_txbufs[afep->afe_txreclaim] = NULL;
		afep->afe_txavail++;
		afep->afe_txreclaim++;
		afep->afe_txreclaim %= AFE_TXRING;
	}
}

static mblk_t *
afe_read(afe_t *afep, int index, unsigned status)
{
	unsigned		length;
	afe_buf_t		*bufp;
	int			good = 1;
	mblk_t			*mp;

	bufp = afep->afe_rxbufs[index];
	length = AFE_RXLENGTH(status);

	/* discard the ethernet frame checksum */
	length -= ETHERFCSL;

	if ((status & AFE_RXSTAT_LAST) == 0) {
		/* its an oversize packet!  ignore it for now */
		DBG(AFE_DRECV, "rx oversize packet");
		return (NULL);
	}

	if (status & AFE_RXSTAT_DESCERR) {
		DBG(AFE_DRECV, "rx descriptor error "
		    "(index = %d, length = %d)", index, length);
		afep->afe_macrcv_errors++;
		good = 0;
	}

	if (status & AFE_RXSTAT_RUNT) {
		DBG(AFE_DRECV, "runt frame (index = %d, length =%d)",
		    index, length);
		afep->afe_runt++;
		afep->afe_macrcv_errors++;
		good = 0;
	}
	if ((status & AFE_RXSTAT_FIRST) == 0) {
		/*
		 * this should also be a toolong, but specifically we
		 * cannot send it up, because we don't have the whole
		 * frame.
		 */
		DBG(AFE_DRECV, "rx fragmented, dropping it");
		good = 0;
	}
	if (status & (AFE_RXSTAT_TOOLONG | AFE_RXSTAT_WATCHDOG)) {
		DBG(AFE_DRECV, "rx toolong or watchdog seen");
		afep->afe_toolong_errors++;
		good = 0;
	}
	if (status & AFE_RXSTAT_COLLSEEN) {
		DBG(AFE_DRECV, "rx late collision");
		/* this should really be rx_late_collisions */
		afep->afe_collisions++;
		good = 0;
	}
	if (status & AFE_RXSTAT_DRIBBLE) {
		DBG(AFE_DRECV, "rx dribbling");
		afep->afe_align_errors++;
		good = 0;
	}
	if (status & AFE_RXSTAT_CRCERR) {
		DBG(AFE_DRECV, "rx frame crc error");
		afep->afe_fcs_errors++;
		good = 0;
	}
	if (status & AFE_RXSTAT_OFLOW) {
		DBG(AFE_DRECV, "rx fifo overflow");
		afep->afe_overflow++;
		afep->afe_macrcv_errors++;
		good = 0;
	}

	if (length > ETHERMAX) {
		/* chip garbled length in descriptor field? */
		DBG(AFE_DRECV, "packet length too big (%d)", length);
		afep->afe_toolong_errors++;
		good = 0;
	}

	/* last fragment in packet, do the bookkeeping */
	if (!good) {
		afep->afe_errrcv++;
		/* packet was munged, drop it */
		DBG(AFE_DRECV, "dropping frame, status = 0x%x", status);
		return (NULL);
	}

	/* sync the buffer before we look at it */
	SYNCBUF(bufp, length, DDI_DMA_SYNC_FORCPU);

	/*
	 * FIXME: note, for efficiency we may wish to "loan-up"
	 * buffers, but for now we just use mblks and copy it.
	 */
	if ((mp = allocb(length + AFE_HEADROOM, BPRI_LO)) == NULL) {
		afep->afe_norcvbuf++;
		return (NULL);
	}

	/* offset by headroom (should be 2 modulo 4), avoids bcopy in IP */
	mp->b_rptr += AFE_HEADROOM;
	bcopy((char *)bufp->bp_buf, mp->b_rptr, length);
	mp->b_wptr = mp->b_rptr + length;

	return (mp);
}

/*
 * Streams and DLPI utility routines.  (Duplicated in strsun.h and
 * sundlpi.h, but we cannot use those for DDI compatibility.)
 */

static int
afe_msgsize(mblk_t *mp)
{
	int n;
	for (n = 0; mp != NULL; mp = mp->b_cont) {
		n += MBLKL(mp);
	}
	return (n);
}

static void
afe_miocack(queue_t *wq, mblk_t *mp, uint8_t type, int count, int error)
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

/*
 * Statistics support.
 */
static int
afe_get_stats(gld_mac_info_t *macinfo, struct gld_stats *sp)
{
	afe_t	*afep = (afe_t *)macinfo->gldm_private;

#if 0
	mutex_enter(&afep->afe_xmtlock);
	if (afep->afe_flags & AFE_RUNNING) {
		/* reclaim tx bufs to pick up latest stats */
		afe_reclaim(afep);
	}
	mutex_exit(&afep->afe_xmtlock);

	/* update the missed frame count from CSR */
	if (afep->afe_flags & AFE_RUNNING) {
		afep->afe_missed += GETCSR(afep, AFE_CSR_LPC) & AFE_LPC_COUNT;
	}
#endif

	sp->glds_speed = afep->afe_ifspeed;
	sp->glds_media = afep->afe_media;
	sp->glds_intr = afep->afe_intr;
	sp->glds_norcvbuf = afep->afe_norcvbuf;
	sp->glds_errrcv = afep->afe_errrcv;
	sp->glds_errxmt = afep->afe_errxmt;
	sp->glds_missed = afep->afe_missed;
	sp->glds_underflow = afep->afe_underflow;
	sp->glds_overflow = afep->afe_overflow;
	sp->glds_frame = afep->afe_align_errors;
	sp->glds_crc = afep->afe_fcs_errors;
	sp->glds_duplex = afep->afe_duplex;
	sp->glds_nocarrier = afep->afe_carrier_errors;
	sp->glds_collisions = afep->afe_collisions;
	sp->glds_excoll = afep->afe_ex_collisions;
	sp->glds_xmtlatecoll = afep->afe_tx_late_collisions;
	sp->glds_defer = afep->afe_defer_xmts;
	sp->glds_dot3_first_coll = afep->afe_first_collisions;
	sp->glds_dot3_multi_coll = afep->afe_multi_collisions;
	sp->glds_dot3_sqe_error = afep->afe_sqe_errors;
	sp->glds_dot3_mac_xmt_error = afep->afe_macxmt_errors;
	sp->glds_dot3_mac_rcv_error = afep->afe_macrcv_errors;
	sp->glds_dot3_frame_too_long = afep->afe_toolong_errors;
	sp->glds_short = afep->afe_runt;

	return (0);
}

/*
 * NDD support.
 */
static afe_nd_t *
afe_ndfind(afe_t *afep, char *name)
{
	afe_nd_t	*ndp;

	for (ndp = afep->afe_ndp; ndp != NULL; ndp = ndp->nd_next) {
		if (!strcmp(name, ndp->nd_name)) {
			break;
		}
	}
	return (ndp);
}

static void
afe_ndadd(afe_t *afep, char *name, afe_nd_pf_t get, afe_nd_pf_t set,
    intptr_t arg1, intptr_t arg2)
{
	afe_nd_t	*newndp;
	afe_nd_t	**ndpp;

	newndp = (afe_nd_t *)kmem_alloc(sizeof (afe_nd_t), KM_SLEEP);
	newndp->nd_next = NULL;
	newndp->nd_name = name;
	newndp->nd_get = get;
	newndp->nd_set = set;
	newndp->nd_arg1 = arg1;
	newndp->nd_arg2 = arg2;

	/* seek to the end of the list */
	for (ndpp = &afep->afe_ndp; *ndpp; ndpp = &(*ndpp)->nd_next) {
	}

	*ndpp = newndp;
}

static void
afe_ndempty(mblk_t *mp)
{
	while (mp != NULL) {
		mp->b_rptr = mp->b_datap->db_base;
		mp->b_wptr = mp->b_rptr;
		/* bzero(mp->b_wptr, MBLKSIZE(mp));*/
		mp = mp->b_cont;
	}
}

static void
afe_ndget(afe_t *afep, queue_t *wq, mblk_t *mp)
{
	mblk_t		*nmp = mp->b_cont;
	afe_nd_t	*ndp;
	int		rv;
	char		name[128];

	/* assumption, name will fit in first mblk of chain */
	if ((nmp == NULL) || (MBLKSIZE(nmp) < 1)) {
		afe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		return;
	}

	if (afe_ndparselen(nmp) >= sizeof (name)) {
		afe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		return;
	}
	afe_ndparsestring(nmp, name, sizeof (name));

	/* locate variable */
	if ((ndp = afe_ndfind(afep, name)) == NULL) {
		afe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		return;
	}

	/* locate set callback */
	if (ndp->nd_get == NULL) {
		afe_miocack(wq, mp, M_IOCNAK, 0, EACCES);
		return;
	}

	/* clear the result buffer */
	afe_ndempty(nmp);

	rv = (*ndp->nd_get)(afep, nmp, ndp);
	if (rv == 0) {
		/* add final null bytes */
		rv = afe_ndaddbytes(nmp, "\0", 1);
	}

	if (rv == 0) {
		afe_miocack(wq, mp, M_IOCACK, afe_msgsize(nmp), 0);
	} else {
		afe_miocack(wq, mp, M_IOCNAK, 0, rv);
	}
}

static void
afe_ndset(afe_t *afep, queue_t *wq, mblk_t *mp)
{
	mblk_t		*nmp = mp->b_cont;
	afe_nd_t	*ndp;
	int		rv;
	char		name[128];

	/* assumption, name will fit in first mblk of chain */
	if ((nmp == NULL) || (MBLKSIZE(nmp) < 1)) {
		return;
	}

	if (afe_ndparselen(nmp) >= sizeof (name)) {
		afe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
	}
	afe_ndparsestring(nmp, name, sizeof (name));

	/* locate variable */
	if ((ndp = afe_ndfind(afep, name)) == NULL) {
		afe_miocack(wq, mp, M_IOCNAK, 0, EINVAL);
		return;
	}

	/* locate set callback */
	if (ndp->nd_set == NULL) {
		afe_miocack(wq, mp, M_IOCNAK, 0, EACCES);
		return;
	}

	rv = (*ndp->nd_set)(afep, nmp, ndp);

	if (rv == 0) {
		afe_miocack(wq, mp, M_IOCACK, 0, 0);
	} else {
		afe_miocack(wq, mp, M_IOCNAK, 0, rv);
	}
}

static int
afe_ndaddbytes(mblk_t *mp, char *bytes, int cnt)
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
afe_ndaddstr(mblk_t *mp, char *str, int addnull)
{
	/* store the string, plus the terminating null */
	return (afe_ndaddbytes(mp, str, strlen(str) + (addnull ? 1 : 0)));
}

static int
afe_ndparselen(mblk_t *mp)
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
afe_ndparseint(mblk_t *mp)
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
				afe_t *afep = NULL;
				DBG(AFE_DPHY, "parsed value %d", val);
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
afe_ndparsestring(mblk_t *mp, char *buf, int maxlen)
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
afe_ndquestion(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	for (ndp = afep->afe_ndp; ndp; ndp = ndp->nd_next) {
		int	rv;
		char	*s;
		if ((rv = afe_ndaddstr(mp, ndp->nd_name, 0)) != 0) {
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
		if ((rv = afe_ndaddstr(mp, s, 1)) != 0) {
			return (rv);
		}
	}
	return (0);
}

static int
afe_ndgetlinkstatus(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	unsigned	val;
	mutex_enter(&afep->afe_xmtlock);
	val = afep->afe_linkup;
	mutex_exit(&afep->afe_xmtlock);

	return (afe_ndaddstr(mp, val ? "1" : "0", 1));
}

static int
afe_ndgetlinkspeed(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	unsigned	val;
	char		buf[16];
	mutex_enter(&afep->afe_xmtlock);
	val = afep->afe_ifspeed;
	mutex_exit(&afep->afe_xmtlock);
	/* convert from bps to Mbps */
	sprintf(buf, "%d", val / 1000000);
	return (afe_ndaddstr(mp, buf, 1));
}

static int
afe_ndgetlinkmode(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	unsigned	val;
	mutex_enter(&afep->afe_xmtlock);
	val = afep->afe_duplex;
	mutex_exit(&afep->afe_xmtlock);
	return (afe_ndaddstr(mp, val == GLD_DUPLEX_FULL ? "1" : "0", 1));
}

static int
afe_ndgetsrom(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	unsigned	val;
	int		i;
	char		buf[80];

	for (i = 0; i < (1 << afep->afe_sromwidth); i++) {
		val = afe_readsromword(afep, i);
		sprintf(buf, "%s%04x", i % 8 ? " " : "", val);
		afe_ndaddstr(mp, buf, ((i % 8) == 7) ? 1 : 0);
	}
	return (0);
}

static int
afe_ndgetstring(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	char	*s = (char *)ndp->nd_arg1;
	return (afe_ndaddstr(mp, s ? s : "", 1));
}

static int
afe_ndgetmiibit(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	unsigned	val;
	unsigned	mask;
	int		reg;

	reg = (int)ndp->nd_arg1;
	mask = (unsigned)ndp->nd_arg2;

	mutex_enter(&afep->afe_xmtlock);
	if (afep->afe_flags & AFE_SUSPENDED) {
		mutex_exit(&afep->afe_xmtlock);
		/* device is suspended */
		return (EIO);
	}
	val = afe_miiread(afep, afep->afe_phyaddr, reg);
	mutex_exit(&afep->afe_xmtlock);

	return (afe_ndaddstr(mp, val & mask ? "1" : "0", 1));
}

static int
afe_ndgetadv(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	unsigned	val;

	mutex_enter(&afep->afe_xmtlock);
	val = *((unsigned *)ndp->nd_arg1);
	mutex_exit(&afep->afe_xmtlock);

	return (afe_ndaddstr(mp, val ? "1" : "0", 1));
}

static int
afe_ndsetadv(afe_t *afep, mblk_t *mp, afe_nd_t *ndp)
{
	unsigned	*ptr = (unsigned *)ndp->nd_arg1;

	mutex_enter(&afep->afe_xmtlock);

	*ptr = (afe_ndparseint(mp) ? 1 : 0);
	/* now reset the phy */
	if ((afep->afe_flags & AFE_SUSPENDED) == 0) {
		afe_phyinit(afep);
	}
	mutex_exit(&afep->afe_xmtlock);

	return (0);
}

static void
afe_ndfini(afe_t *afep)
{
	afe_nd_t	*ndp;

	while ((ndp = afep->afe_ndp) != NULL) {
		afep->afe_ndp = ndp->nd_next;
		kmem_free(ndp, sizeof (afe_nd_t));
	}
}

static void
afe_ndinit(afe_t *afep)
{
	afe_ndadd(afep, "?", afe_ndquestion, NULL, 0, 0);
	afe_ndadd(afep, "model", afe_ndgetstring, NULL,
	    (intptr_t)afep->afe_cardp->card_cardname, 0);
	afe_ndadd(afep, "link_status", afe_ndgetlinkstatus, NULL, 0, 0);
	afe_ndadd(afep, "link_speed", afe_ndgetlinkspeed, NULL, 0, 0);
	afe_ndadd(afep, "link_mode", afe_ndgetlinkmode, NULL, 0, 0);
	afe_ndadd(afep, "driver_version", afe_ndgetstring, NULL,
	    (intptr_t)afe_version, 0);
	afe_ndadd(afep, "adv_autoneg_cap", afe_ndgetadv, afe_ndsetadv,
	    (intptr_t)&afep->afe_adv_aneg, 0);
	afe_ndadd(afep, "adv_100T4_cap", afe_ndgetadv, afe_ndsetadv,
	    (intptr_t)&afep->afe_adv_100T4, 0);
	afe_ndadd(afep, "adv_100fdx_cap", afe_ndgetadv, afe_ndsetadv,
	    (intptr_t)&afep->afe_adv_100fdx, 0);
	afe_ndadd(afep, "adv_100hdx_cap", afe_ndgetadv, afe_ndsetadv,
	    (intptr_t)&afep->afe_adv_100hdx, 0);
	afe_ndadd(afep, "adv_10fdx_cap", afe_ndgetadv, afe_ndsetadv,
	    (intptr_t)&afep->afe_adv_10fdx, 0);
	afe_ndadd(afep, "adv_10hdx_cap", afe_ndgetadv, afe_ndsetadv,
	    (intptr_t)&afep->afe_adv_10hdx, 0);
	afe_ndadd(afep, "autoneg_cap", afe_ndgetmiibit, NULL,
	    MII_REG_BMSR, MII_BMSR_ANA);
	afe_ndadd(afep, "100T4_cap", afe_ndgetmiibit, NULL,
	    MII_REG_BMSR, MII_BMSR_100BT4);
	afe_ndadd(afep, "100fdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_BMSR, MII_BMSR_100FDX);
	afe_ndadd(afep, "100hdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_BMSR, MII_BMSR_100HDX);
	afe_ndadd(afep, "10fdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_BMSR, MII_BMSR_10FDX);
	afe_ndadd(afep, "10hdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_BMSR, MII_BMSR_10HDX);
	afe_ndadd(afep, "lp_autoneg_cap", afe_ndgetmiibit, NULL,
	    MII_REG_ANER, MII_ANER_LPANA);
	afe_ndadd(afep, "lp_100T4_cap", afe_ndgetmiibit, NULL,
	    MII_REG_ANLPAR, MII_ANEG_100BT4);
	afe_ndadd(afep, "lp_100fdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_ANLPAR, MII_ANEG_100FDX);
	afe_ndadd(afep, "lp_100hdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_ANLPAR, MII_ANEG_100HDX);
	afe_ndadd(afep, "lp_10fdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_ANLPAR, MII_ANEG_10FDX);
	afe_ndadd(afep, "lp_10hdx_cap", afe_ndgetmiibit, NULL,
	    MII_REG_ANLPAR, MII_ANEG_10HDX);
	afe_ndadd(afep, "srom", afe_ndgetsrom, NULL, 0, 0);
}

/*
 * Debugging and error reporting.
 */
static void
afe_verror(dev_info_t *dip, int level, char *fmt, va_list ap)
{
	char	buf[256];
	vsprintf(buf, fmt, ap);
	if (dip) {
		cmn_err(level, level == CE_CONT ? "%s%d: %s\n" : "%s%d: %s",
		    ddi_driver_name(dip), ddi_get_instance(dip), buf);
	} else {
		cmn_err(level, level == CE_CONT ? "%s: %s\n" : "%s: %s",
		    AFE_IDNAME, buf);
	}
}

static void
afe_error(dev_info_t *dip, char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	afe_verror(dip, CE_WARN, fmt, ap);
	va_end(ap);
}

#ifdef	DEBUG

static void
afe_dprintf(afe_t *afep, const char *func, int level, char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	if (afe_debug & level) {
		char	tag[64];
		char	buf[256];

		if (afep && afep->afe_dip) {
			sprintf(tag, "%s%d", ddi_driver_name(afep->afe_dip),
			    ddi_get_instance(afep->afe_dip));
		} else {
			sprintf(tag, "%s", AFE_IDNAME);
		}

		sprintf(buf, "%s: %s: %s", tag, func, fmt);

		vcmn_err(CE_CONT, buf, ap);
	}
	va_end(ap);
}

#endif
