/*
 * Diagnostic utility for ethernet cards using Garrett's drivers.
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

#ident	"@(#)$Id: etherdiag.c,v 1.3 2005/11/27 01:10:05 gdamore Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/dlpi.h>
#include <stropts.h>
#include <libdevinfo.h>
#include <kstat.h>
#include <sys/afe.h>

/* use functional form to keep gcc -Wall silent */

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
	exit(1);
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
	(void) sprintf(end, "%s", name);
	end += strlen(end) + 1;

	(void) sprintf(end, "%s", val);
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

	(void) sprintf(buf, "%s", name);
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
			(void) printf("\n");
		}
		if (strlen(line) == 0) {
			continue;
		}
		if (strcmp(name, "?") != 0) {
			(void) printf("%s%s", verbose ? "    " : "", line);
		} else {
			char 	*p, *m, *e;
			p = strtok_r(line, " \t", &e);
			m = strtok_r(NULL, "\n", &e);
			(void) printf("%s%-30s%s",
			    verbose ? "    " : "", p, m);
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
			(void) printf("\n");
		}
		if (verbose) {
			(void) printf("%s:\n", n);
		}
		ndd_get(fd, n, verbose);

	next:
		options = NULL;
	}
}

static int
getpromprop(di_node_t node, const char *name)
{
	di_prom_handle_t	ph;
	int			*vals = NULL;
	int			val = -1;
	int			nval;
	ph = di_prom_init();
	if (ph == DI_PROM_HANDLE_NIL) {
		perror("prom_init");
		return (-1);
	}
	nval = di_prom_prop_lookup_ints(ph, node, name, &vals);
	if (nval > 0) {
		val = vals[0];
	}
	di_prom_fini(ph);
	return (val);
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
	int		vid, did, svid, sdid;
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


	/* we look at prom properties to get PCI config info */
	vid = getpromprop(node, "vendor-id");
	did = getpromprop(node, "device-id");
	svid = getpromprop(node, "subsystem-vendor-id");
	sdid = getpromprop(node, "subsystem-id");
	rid = getpromprop(node, "revision-id");
	slot = getpromprop(node, "slot");

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
forallinterfaces(const char *driver, int (*func)(di_node_t, void *),
    void *arg)
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
	int		ppa = 0;
	int		mult = 1;
	char		c;

	(void) snprintf(devbuf, sizeof (devbuf), "%s", dev);
	idx = strlen(devbuf) - 1;
	while ((idx > 0) && (c = devbuf[idx]) && (c >= '0') && (c <= '9')) {
		c -= '0';
		ppa += mult * c;
		mult *= 10;
		devbuf[idx] = 0;
		idx--;
	}

	node = di_drv_first_node(devbuf, di_init("/", DINFOCPYALL));

	while (node != DI_NODE_NIL) {
		if (di_instance(node) == ppa) {
			(void) showinterface(node, (void *)verbose);
			break;
		}
		node = di_drv_next_node(node);
	}
}

int
main(int argc, char **argv)
{
	int		opt;
	int		fd;
	char		*dev = NULL;
	char		*optn = NULL;
	intptr_t	optv = 0;

	/* we parse argv twice, first we look for the PPA */
	while ((opt = getopt(argc, argv, "d:vEn:")) != -1) {
		switch (opt) {
		case 'd':
			dev = optarg;
			break;
		case 'v':
			optv++;
			break;
		case 'n':
			optn = optarg;
			break;
		default:
			usage();
		}
	}

	if ((!dev) && (!optn)) {

		/* iterate over all interfaces */
		(void) forallinterfaces("afe", showinterface, (void *)optv);
		(void) forallinterfaces("mxfe", showinterface, (void *)optv);
		return (0);
	}

	/* must have device named at this point */
	if (!dev) {
		usage();
	}

	if ((fd = openattach(dev)) < 0) {
		exit(-1);
	}

	if (optn) {
		do_ndd(fd, optn, optv);

	} else {
		displaydev(dev, optv);
	}
		
	return (0);
}
