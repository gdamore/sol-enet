/*
 * Diagnostic utility for ethernet cards using Garrett's drivers.
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

#ident	"@(#)$Id: etherdiag.c,v 1.2 2004/08/27 22:58:49 gdamore Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/dlpi.h>
#include <stropts.h>
#include <ctype.h>
#include <libdevinfo.h>
#include <kstat.h>
#include <sys/afe.h>

/* use functional form to keep gcc -Wall silent */
#undef	isdigit

#define	min(x,y) ((y) > (x) ? (x) : (y))

static void usage(void);

static int
openattach(char *dev)
{
	int	fd;
	char	buf[128];

	if (dev == NULL) {
		usage();
	}

	(void) snprintf(buf, sizeof (buf), "/dev/%s", dev);
	if ((fd = open(buf, O_RDWR)) < 0) {
		(void) snprintf(buf, sizeof (buf), "open(/dev/%s)", dev);
		perror(buf);
		return (-1);
	}

	return (fd);
}

static int
getcsr(int fd, unsigned offset, unsigned *value)
{
	struct afe_ioc_csr	csr;
	struct strioctl		str;

	csr.csr_offset = offset;
	csr.csr_value = 0;
	str.ic_cmd = AFEIOC_GETCSR;
	str.ic_timout = -1;
	str.ic_dp = (char *)&csr;
	str.ic_len = sizeof (csr);

	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
		return (-1);
	}

	*value = csr.csr_value;
	return (0);
}

static int
putcsr(int fd, unsigned offset, unsigned value)
{
	struct afe_ioc_csr	csr;
	struct strioctl		str;

	csr.csr_offset = offset;
	csr.csr_value = value;
	str.ic_cmd = AFEIOC_PUTCSR;
	str.ic_timout = -1;
	str.ic_dp = (char *)&csr;
	str.ic_len = sizeof (csr);

	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
		return (-1);
	}

	return (0);
}

static ushort
getmii(int fd, ushort reg)
{
	struct afe_ioc_miireg	mii;
	struct strioctl		str;

	mii.mii_register = reg;
	mii.mii_value = 0;
	str.ic_cmd = AFEIOC_GETMII;
	str.ic_timout = -1;
	str.ic_dp = (char *)&mii;
	str.ic_len = sizeof (mii);

	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
		return (0xffff);
	}

	return (mii.mii_value);
}

static int
putmii(int fd, ushort reg, ushort val)
{
	struct afe_ioc_miireg	mii;
	struct strioctl		str;

	mii.mii_register = reg;
	mii.mii_value = val;
	str.ic_cmd = AFEIOC_GETMII;
	str.ic_timout = -1;
	str.ic_dp = (char *)&mii;
	str.ic_len = sizeof (mii);

	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
		return (-1);
	}

	return (mii.mii_value);
}

static int
geteeprom(int fd, ushort address, ushort *value)
{
	struct afe_ioc_srom	srom;
	struct strioctl		str;

	srom.srom_address = address;
	srom.srom_value = 0;
	str.ic_cmd = AFEIOC_GETSROM;
	str.ic_timout = -1;
	str.ic_dp = (char *)&srom;
	str.ic_len = sizeof (srom);

	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
		return (-1);
	}

	*value = srom.srom_value;
	return (0);
}

static int
getpci(int fd, ushort offset, ushort width, void *resultp)
{
	struct afe_ioc_pcireg	pci;
	struct strioctl		str;

	pci.pci_offset = offset;
	switch (width) {
	case 8:
	case 16:
	case 32:
		pci.pci_width = width;
		break;
	default:
		(void) fprintf(stderr, "bad width specified, assuming 8 bits");
		pci.pci_width = 8;
		return (-1);
	}
	str.ic_cmd = AFEIOC_GETPCI;
	str.ic_timout = -1;
	str.ic_dp = (char *)&pci;
	str.ic_len = sizeof (pci);

	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
		return (-1);
	}

	switch (pci.pci_width) {
	case 8:
		*(uint8_t *)(resultp) = pci.pci_val.pci_val8;
		break;
	case 16:
		*(uint16_t *)(resultp) = pci.pci_val.pci_val16;
		break;
	case 32:
		*(uint32_t *)(resultp) = pci.pci_val.pci_val32;
		break;
	case 64:
		*(uint64_t *)(resultp) = pci.pci_val.pci_val64;
		break;
	}
	return (0);
}

static int
getlinkinfo(char *name, int instance, int *speed, char *duplex, char *media)
{
	kstat_ctl_t	*khp;
	kstat_t		*ksp;
	char		ksname[256];
	int		i;
	int		rv = -1;
	kstat_named_t	*knbuf;

	(void) snprintf(ksname, sizeof (ksname), "%s%d", name, instance);

	if ((khp = kstat_open()) == NULL) {
		return (-1);
	}
	
	if ((ksp = kstat_lookup(khp, name, instance, ksname)) == NULL) {
		goto done;
	}

	if (ksp->ks_type != KSTAT_TYPE_NAMED) {
		(void) fprintf(stderr, "invalid kstat type\n");
		goto done;
	}

	if ((knbuf = calloc(ksp->ks_ndata, sizeof (kstat_named_t))) == NULL) {
		goto done;
	}

	(void) strcpy(duplex, "");
	(void) strcpy(media, "");
	*speed = -1;

	(void) kstat_read(khp, ksp, knbuf);

	for (i = 0; i < ksp->ks_ndata; i++) {
		kstat_named_t *knp = (kstat_named_t *)ksp->ks_data;
		if (knp[i].name == NULL) {
			continue;
		}
		if (strcmp(knp[i].name, "ifspeed") == 0) {
			*speed = (int)((knp[i].value.ui64) / 1000000);

		} else if (strcmp(knp[i].name, "duplex") == 0) {
			(void) strcpy(duplex, knp[i].value.c);

		} else if (strcmp(knp[i].name, "media") == 0) {
			(void) strcpy(media, knp[i].value.c);
		}
	}

	free(knbuf);
	rv = 0;
done:
	(void) kstat_close(khp);
	return (rv);
}

static int
dlpi_get_phys_addr(int fd, t_uscalar_t addr_type, char *addr, int *len)
{
	dl_phys_addr_req_t	*phys_addr_req;
	dl_phys_addr_ack_t	*phys_addr_ack;
	struct strbuf		ctl;
	int			flags = 0;
	char			buffer[512];

	/*LINTED E_PTR_CAST_ALIGN*/
	phys_addr_req = (dl_phys_addr_req_t *) buffer;
	phys_addr_req->dl_primitive = DL_PHYS_ADDR_REQ;
	phys_addr_req->dl_addr_type = addr_type;

	ctl.buf = buffer;
	ctl.len = sizeof (dl_phys_addr_req_t);

	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("putmsg");
		return (-1);
	}

	ctl.maxlen = sizeof (buffer);
	ctl.buf = buffer;
	/*LINTED E_PTR_CAST_ALIGN*/
	phys_addr_ack = (dl_phys_addr_ack_t *) buffer;

	if (getmsg(fd, &ctl, NULL, &flags) < 0) {
		perror("getmsg");
	}

	if (phys_addr_ack->dl_primitive != DL_PHYS_ADDR_ACK) {
		perror("DLPI error: expected DL_PHYS_ADDR_ACK");
		return (-1);
	}

	(void) memcpy(addr, buffer + phys_addr_ack->dl_addr_offset,
	    min(*len, phys_addr_ack->dl_addr_length)); 
	*len = phys_addr_ack->dl_addr_length;
	return (0);
}

static void
printaddr(char *addr, int len)
{
	int idx = 0;
	while (idx < len) {
		(void) printf("%x", (unsigned char) addr[idx]);
		idx++;
		if (idx < len) {
			(void) printf(":");
		}
	}
	(void) printf("\n");
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "usage: etherdiag [-d <interface>] [-v] [-v]\n");
	(void) fprintf(stderr,
	    "       etherdiag -d <interface> -n <parameters>\n");
	(void) fprintf(stderr,
	    "       etherdiag -d <interface> -E\n");
#ifdef	DEBUG
	(void) fprintf(stderr,
	    "       etherdiag -d <interface> -c <csr> [-w <value>]\n");
	(void) fprintf(stderr,
	    "       etherdiag -d <interface> -o <csr-offset> [-w <value>]\n");
	(void) fprintf(stderr,
	    "       etherdiag -d <interface> -m <mii-reg> [-w <value>]\n");
	(void) fprintf(stderr,
	    "       etherdiag -d <interface> -p <pci-config-offset> "
	    "-z 8|16|32|64\n");
#endif
	exit(1);
}

static void
do_getcsr(int fd, unsigned offset)
{
	unsigned val;
	if (getcsr(fd, offset, &val) == 0) {
		if (offset & 0x7) {
			(void) printf("CSR ? @ 0x%02x: 0x%x\n", offset, val);
		} else {
			(void) printf("CSR %d @ 0x%02x: 0x%x\n", 
			   offset >> 3, offset, val);
		}
	}
}

/*
 * Like getsubopt(), but doesn't require option array.  The resulting
 * name and value are returned.  Returns true if parse was success,
 * false when no more suboptions exist.  The string suboptions is
 * altered (null bytes placed at appropriate locations.) 
 */
static int
parsesubopt(char *suboptions, char **namep, char **valuep, char **eptr)
{
	*namep = strtok_r(suboptions, ",", eptr);
	if (*namep != NULL) {
		char	*dummy;
		strtok_r(*namep, "=", &dummy);
		*valuep = strtok_r(NULL, "=", &dummy);
		return (1);
	}
	return (0);
}

static void
ndd_set(int fd, char *name, char *val)
{
	char			buf[8192];
	struct strioctl		str;
	char			*end;

	str.ic_cmd = NDIOC_SET;
	str.ic_timout = -1;
	str.ic_dp = buf;
	str.ic_len = sizeof (buf);

	end = buf;
	sprintf(end, "%s", name);
	end += strlen(end) + 1;

	sprintf(end, "%s", val);
	end += strlen(end) + 1;

	*end = 0;
	end++;

	str.ic_len = (intptr_t)end - (intptr_t)buf;

	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
	}
}

static void
ndd_get(int fd, char *name, intptr_t verbose)
{
	char			buf[8192];
	struct strioctl		str;
	char			*out;
	char			*end;

	str.ic_cmd = NDIOC_GET;
	str.ic_timout = -1;
	str.ic_dp = buf;
	str.ic_len = sizeof (buf);

	sprintf(buf, "%s", name);
	buf[strlen(buf) + 1]  = 0;
	if (ioctl(fd, I_STR, &str) < 0) {
		perror("ioctl");
		return;
	}

	end = buf + str.ic_len;
	out = buf;
	while (out < end) {
		char *line = out;
		out += strlen(out) + 1;
		if (line != buf) {
			printf("\n");
		}
		if (strlen(line) == 0) {
			continue;
		}
		if (strcmp(name, "?") != 0) {
			printf("%s%s", verbose ? "    " : "", line);
		} else {
			char 	*p, *m, *e;
			p = strtok_r(line, " \t", &e);
			m = strtok_r(NULL, "\n", &e);
			printf("%s%-30s%s", verbose ? "    " : "", p, m);
		}
	}
}

static void
do_ndd(int fd, char *options, intptr_t verbose)
{
	char	*n, *v, *e;

	while (parsesubopt(options, &n, &v, &e)) {

		if (v) {
			ndd_set(fd, n, v);
			if (!verbose) {
				goto next;
			}
		} 
		if (options == NULL) {
			printf("\n");
		}
		if (verbose) {
			printf("%s:\n", n);
		}
		ndd_get(fd, n, verbose);

	next:
		options = NULL;
	}
}

static void
do_dumpeeprom(int fd)
{
	int	i;
	ushort	buf[2048];
	ushort	swapped[2048];

	(void) memset(buf, 0, sizeof (buf));
	for (i = 0; i < 2048; i++) {
		/*LINTED E_PTR_CAST_ALIGN*/
		(void) geteeprom(fd, i, buf + i);
	}

	swab((caddr_t)buf, (caddr_t)swapped, sizeof (buf));

	for (i = 0; i < 2048; i += 8) {
		/*LINTED E_PTR_CAST_ALIGN*/
		unsigned short	*u = (swapped + i);
		(void) printf("%04x: "
		    "%04x %04x %04x %04x %04x %04x %04x %04x\n",
		    i * 2, u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7]);
	}
}

static void
do_getmii(int fd, ushort reg)
{
	(void) printf("MII %d: 0x%x\n", reg, getmii(fd, reg));
}

static void
do_putcsr(int fd, unsigned offset, unsigned val)
{
	if (putcsr(fd, offset, val) == 0) {
		if (offset & 0x7) {
			(void) printf("CSR ? @ 0x%02x: 0x%x (write)\n",
			    offset, val);
		} else {
			(void) printf("CSR %d @ 0x%02x: 0x%x (write)\n",
			    offset >> 3, offset, val);
		}
	}
}

static void
do_putmii(int fd, ushort reg, ushort val)
{
	if (putmii(fd, reg, val)) {
		(void) printf("MII %d: 0x%x (write)\n", reg, val);
	}
}

static void
do_getpci(int fd, ushort offset, ushort width)
{
	uint8_t val8;
	uint16_t val16;
	uint32_t val32;
	uint64_t val64;

	switch (width) {
	case 8:
		if (getpci(fd, offset, width, &val8) == 0) {
			(void) printf("PCI @ 0x%02x: 0x%x (8 bits)\n",
			    offset, val8);
		}
		break;
	case 16:
		if (getpci(fd, offset, width, &val16) == 0) {
			(void) printf("PCI @ 0x%02x: 0x%x (16 bits)\n",
			    offset, val16);
		}
		break;
	case 32:
		if (getpci(fd, offset, width, &val32) == 0) {
			(void) printf("PCI @ 0x%02x: 0x%x (32 bits)\n",
			    offset, val32);
		}
		break;
	case 64:
		if (getpci(fd, offset, width, &val64) == 0) {
			(void) printf("PCI @ 0x%02x: 0x%llx (32 bits)\n",
			    offset, (long long)val64);
		}
		break;
	}
}

static int
getslot(di_node_t node)
{
	di_prom_handle_t	ph;
	int			*slots = NULL;
	int			slot = -1;
	int			nslots;

	ph = di_prom_init();
	if (ph == DI_PROM_HANDLE_NIL) {
		perror("prom_init");
		return (-1);
	}
	nslots = di_prom_prop_lookup_ints(ph, node, "slot", &slots);
	if (nslots > 0) {
		slot = slots[0];
	}
	di_prom_fini(ph);
	return (slot);
}

static int 
showinterface(di_node_t node, void *arg)
{
	intptr_t	verbose = (intptr_t)arg;
	int		fd;
	int		instance;
	char		*name;
	char		*path;
	char		iface[32];
	uint16_t	vid, did, svid, sdid;
	uint8_t		rid;
	int		slot;
	char		*model = NULL;
	char		fact_addr[6];
	char		curr_addr[6];
	int		len;
	char		media[128];
	char		duplex[128];
	int		speed;

	name = di_driver_name(node);
	instance = di_instance(node);
	path = di_devfs_path(node);
	(void) sprintf(iface, "%s%d", name, instance);
	fd = openattach(iface);
	if (fd < 0) {
		return (-1);
	}
	(void) sprintf(iface, "%s%d", name, instance);

	if ((getpci(fd, AFE_PCI_VID, 16, &vid) != 0) ||
	    (getpci(fd, AFE_PCI_DID, 16, &did) != 0) ||
	    (getpci(fd, AFE_PCI_RID, 8, &rid) != 0) ||
	    (getpci(fd, AFE_PCI_SVID, 16, &svid) != 0) ||
	    (getpci(fd, AFE_PCI_SSID, 16, &sdid) != 0)) {
		(void) close(fd);
		return (-1);
	}

	slot = getslot(node);

	len = 6;
	if (dlpi_get_phys_addr(fd, DL_FACT_PHYS_ADDR, fact_addr, &len) < 0) {
		(void) memset(fact_addr, 0, 6);
	}
	len = 6;
	if (dlpi_get_phys_addr(fd, DL_CURR_PHYS_ADDR, curr_addr, &len) < 0) {
		(void) memset(curr_addr, 0, 6);
	}
	if (di_prop_lookup_strings(DDI_DEV_T_ANY, node, "model", &model) < 0) {
		/* attempt to lookup model property failed */
		model = NULL;
	}

	(void) getlinkinfo(name, instance, &speed, duplex, media);

	(void) printf("Interface: %s\n", iface);
	(void) printf("Model: %s\n", model ? model : "Unknown");

	(void) printf(speed > 0 ? "Speed: %d Mbps\n" : "Speed: Unknown\n",
	    speed);
	(void) printf("Duplex: %s\n", duplex);
	(void) printf("Media: %s\n", media);

	(void) printf("%s: ",
	    verbose ? "Current Ethernet Address" : "Ethernet Address");
	printaddr(curr_addr, 6);

	if (verbose) {
		(void) printf("Factory Ethernet Address: ");
		printaddr(fact_addr, 6);
		(void) printf("Devices Path: %s\n", path);
		(void) printf("PCI Vendor Id: 0x%04x\n", vid);
		(void) printf("PCI Device Id: 0x%04x\n", did);
		(void) printf("PCI Revision Id: 0x%02x\n", rid);
		(void) printf("PCI Subsystem Vendor Id: 0x%04x\n", svid);
		(void) printf("PCI Subsystem Device Id: 0x%04x\n", sdid);
	}
	if (verbose > 1) {
		if (slot >= 0) {
			(void) printf("PCI Slot: %d\n", slot);
		} else {
			(void) printf("PCI Slot: Unknown\n");
		}
		(void) printf("Binding Name: %s\n", di_binding_name(node));
		(void) printf("Bus Address: %s\n", di_bus_addr(node));
	}
	(void) printf("\n");
	di_devfs_path_free(path);
	return (0);
}

static int
forallinterfaces(const char *driver, int (*func)(di_node_t, void *), void *arg)
{
	di_node_t	node;
	int		temprv, rv = 0;
	int		fd;
	char		path[80];

	/* first we open the general device to ensure everything is attached */
	(void) snprintf(path, sizeof (path), "/dev/%s", driver);
	if ((fd = open(path, O_RDONLY)) >= 0) {
		(void) close(fd);
	}

	/* now recurse down the device tree, looking for matching nodes */
	node = di_drv_first_node(driver, di_init("/", DINFOCPYALL));

	while (node != DI_NODE_NIL) {
		temprv = func(node, arg);
		if (temprv) {
			rv = temprv;
		}
		node = di_drv_next_node(node);
	}
	return (rv);
}

static void
displaydev(char *dev, intptr_t verbose)
{
	char		devbuf[256];
	di_node_t	node;
	int		idx;
	int		ppa;

	(void) strcpy(devbuf, dev);
	idx = strlen(devbuf) - 1;
	while ((idx > 0) && isdigit(devbuf[idx])) {
		idx--;
	}
	idx++;
	ppa = atoi(&devbuf[idx]);
	devbuf[idx] = 0;

	node = di_drv_first_node(devbuf, di_init("/", DINFOCPYALL));
	while (node != NULL) {
		if (di_instance(node) == ppa) {
			(void) showinterface(node, (void *)verbose);
			break;
		}
	}
}

int
main(int argc, char **argv)
{
	int		opt;
	int		fd;
	char		*dev = NULL;
	int		opto = 0;
	int		optc = 0;
	int		optw = 0;
	int		optp = 0;
	int		optm = 0;
	char		*optn = NULL;
	int		optz = 0;
	intptr_t	optv = 0;
	int		optE = 0;
	unsigned	csr = 0;
	unsigned	val = 0;
	unsigned	width = 0;
	unsigned	pciaddr = 0;
	ushort		mii = 0;

#ifdef	DEBUG
#define	DEBUGOPTIONS	"o:c:m:w:p:z:"
#else
#define	DEBUGOPTIONS
#endif

	/* we parse argv twice, first we look for the PPA */
	while ((opt = getopt(argc, argv, "d:vEn:" DEBUGOPTIONS)) != -1) {
		switch (opt) {
		case 'E':
			if (optn || optc || opto || optp || optz || optw ||
			    optp || optm) {
				usage();
			}
			optE++;
			break;
		case 'd':
			dev = optarg;
			break;
		case 'v':
			optv++;
			break;
		case 'n':
			if (optE) {
				usage();
			}
			optn = optarg;
			break;
		case 'c':
			if (optc || optm || opto || optp || optE || optz) {
				usage();
			}
			csr = strtoul(optarg, NULL, 0);
			csr *= 8;
			optc++;
			break;
		case 'o':
			if (optc || optm || opto || optp || optE || optz) {
				usage();
			}
			csr = strtoul(optarg, NULL, 0);
			opto++;
			break;
		case 'p':
			if (optc || optm || opto || optp || optw || optE) {
				usage();
			}
			pciaddr = strtoul(optarg, NULL, 0);
			optp++;
			break;
		case 'z':
			if (optz || optc || optm || opto || optw || optE) {
				usage();
			}
			width = strtoul(optarg, NULL, 0);
			optz++;
			break;
		case 'w':
			if (optw || optz || optE) {
				usage();
			}	
			val = strtoul(optarg, NULL, 0);
			optw++;
			break;
		case 'm':
			if (optc || optm || opto || optp || optz || optE) {
				usage();
			}
			mii = strtoul(optarg, NULL, 0);
			optm++;
			break;
		default:
			usage();
			exit(1);
		}
	}

	if ((!dev) && (!optm) && (!optc) && (!opto) && (!optp) && (!optE)) {

		/* iterate over all interfaces */
		(void) forallinterfaces("afe", showinterface, (void *)optv);
		(void) forallinterfaces("mxfe", showinterface, (void *)optv);
		return (0);
	}
	if ((fd = openattach(dev)) < 0) {
		exit(-1);
	}

	if (optn) {
		do_ndd(fd, optn, optv);
	} else if (optc || opto) {
		if (optw) {
			do_putcsr(fd, csr, val);
		} else {
			do_getcsr(fd, csr);
		}

	} else if (optE) {
		do_dumpeeprom(fd);

	} else if (optm) {
		if (optw) {
			do_putmii(fd, mii, val);
		} else {
			do_getmii(fd, mii);
		}

	} else  if (optp) {
		if (optz) {
			do_getpci(fd, pciaddr, width);
		} else {
			usage();
		}

	} else {
		displaydev(dev, optv);
	}
		
	return (0);
}
