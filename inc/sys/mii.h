/*
 * IEEE 802.3 Media Independent Interface (MII) definitions
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

#ifndef	_MII_H
#define	_MII_H

#ident	"@(#)$Id: mii.h,v 1.2 2004/08/27 23:39:34 gdamore Exp $"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PHY (Transceiver) registers -- names follow DP83840A datasheet
 * These are all standard per the IEEE 802.3 MII spec.
 */
#define	MII_REG_BMCR		0x00	/* control register */
#define	MII_REG_BMSR		0x01	/* status register */
#define	MII_REG_PHYIDR1		0x02	/* vendor id */
#define	MII_REG_PHYIDR2		0x03	/* device id */
#define	MII_REG_ANAR		0x04	/* autonegotiation advertisement */
#define	MII_REG_ANLPAR		0x05	/* link partner abilities */
#define	MII_REG_ANER		0x06	/* autonegotiation expansion */
#define	MII_REG_GCR		0x09	/* gigabit control register */
#define	MII_REG_GSR		0x0a	/* gigabit status register */
#define	MII_REG_ESR		0x0f	/* extended status register */

/*
 * Helpful bits within the PHY registers.
 */
#define	MII_BMCR_RESET		0x8000	/* soft reset */
#define	MII_BMCR_LOOP		0x4000	/* xcvr loopback */
#define	MII_BMCR_SPEED		0x2000	/* 1 = 100Mbps, 0 = 10Mbps */
#define	MII_BMCR_ANEG		0x1000	/* autonegotiation enable */
#define	MII_BMCR_PDOWN		0x0800	/* power down */
#define	MII_BMCR_ISO		0x0400	/* isolate PHY */
#define	MII_BMCR_RANEG		0x0200	/* restart autonegotiation */
#define	MII_BMCR_DUPLEX		0x0100	/* 1 = full, 0 = half */
#define	MII_BMCR_COLTST		0x0080	/* collision test */
#define	MII_BMCR_GIGABIT	0x0040	/* 1 = 1Gbps */	

#define	MII_BMSR_100BT4		0x8000	/* can perform 100 Base-T4 */
#define	MII_BMSR_100FDX		0x4000	/* can perform 100 full duplex */
#define	MII_BMSR_100HDX		0x2000	/* can perform 100 half duplex */
#define	MII_BMSR_10FDX		0x1000	/* can perform 10 full duplex */
#define	MII_BMSR_10HDX		0x0800	/* can perform 10 half duplex */
#define	MII_BMSR_ES		0x0100	/* extended status */
#define	MII_BMSR_SUPPREAMBLE	0x0040	/* preamble suppression */
#define	MII_BMSR_ANC		0x0020	/* autonegotiation complete */
#define	MII_BMSR_RFAULT		0x0010	/* remote fault */
#define	MII_BMSR_ANA		0x0008	/* can perform autonegotiation */
#define	MII_BMSR_LINK		0x0004	/* link state (1 = up, 0 = down) */
#define	MII_BMSR_JABBER		0x0002	/* jabber condition detected */
#define	MII_BMSR_EXTCAP		0x0001	/* extended capabilities */

#define	MII_ANEG_NPAGE		0x8000	/* autoneg next page */
#define	MII_ANEG_ACK		0x4000	/* autoneg acknowledge */
#define	MII_ANEG_RFAULT		0x2000	/* autoneg remote fault */
#define	MII_ANEG_ASMPAUSE	0x0800	/* autoneg asymmetric pause */
#define	MII_ANEG_PAUSE		0x0400	/* autoneg pause */
#define	MII_ANEG_100BT4		0x0200	/* autoneg 100 Base-T4 */
#define	MII_ANEG_100FDX		0x0100	/* autoneg 100 Base-T, full duplex */
#define	MII_ANEG_100HDX		0x0080	/* autoneg 100 Base-T, half duplex */
#define	MII_ANEG_10FDX		0x0040	/* autoneg 10 Base-T, full duplex */
#define	MII_ANEG_10HDX		0x0020	/* autoneg 10 Base-T, half duplex */
#define	MII_ANEG_PROTOCOL	0x001f	/* autoneg protocol */
#define	MII_ANEG_8023		0x0001	/* autoneg 802.3 protocol */

#define	MII_ANER_PDF		0x0010	/* parallel detection fault */
#define	MII_ANER_LPNP		0x0008	/* link partner next-page-able */
#define	MII_ANER_NP		0x0004	/* next-page-able */
#define	MII_ANER_PAGERX		0x0002	/* page received */
#define	MII_ANER_LPANA		0x0001	/* link partner autonegotiable */

#ifdef	_KERNEL
/*
 * Put exported kernel interfaces here.  (There should be none.)
 */
#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _MII_H */
