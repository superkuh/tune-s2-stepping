/* tune-s2 -- simple zapping tool for the Linux DVB S2 API
 *
 * UDL (updatelee@gmail.com)
 * Derived from work by:
 * 	Igor M. Liplianin (liplianin@me.by)
 * 	Alex Betis <alex.betis@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "tune-s2.h"

#if DVB_API_VERSION < 5
#error tune-s2 requires Linux DVB driver API version 5.0 or newer!
#endif

int name2value(char *name, struct options *table)
{
	while (table->name) {
		if (!strcmp(table->name, name))
			return table->value;
		table++;
	}
}

char * value2name(int value, struct options *table)
{
	while (table->name) {
		if (table->value == value)
			return table->name;
		table++;
	}
}

int check_frontend (int frontend_fd)
{
	fe_status_t status;
	uint16_t snr, signal;
	uint32_t ber, uncorrected_blocks;

	if (ioctl(frontend_fd, FE_READ_STATUS, &status) == -1)
		perror("FE_READ_STATUS failed");

	/* some frontends might not support all these ioctls, thus we
	 * avoid printing errors
	 */
	if (ioctl(frontend_fd, FE_READ_SIGNAL_STRENGTH, &signal) == -1)
		signal = -2;
	if (ioctl(frontend_fd, FE_READ_SNR, &snr) == -1)
		snr = -2;
	if (ioctl(frontend_fd, FE_READ_BER, &ber) == -1)
		ber = -2;
	if (ioctl(frontend_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks) == -1)
		uncorrected_blocks = -2;
	printf ("status %s | signal %3u%% | snr %2.1f db | ber %d | unc %d | ",
			(status & FE_HAS_LOCK) ? "Locked" : "Unlocked", (signal * 100) / 0xffff, (snr / 2560.0), ber, uncorrected_blocks);
	if (status & FE_HAS_LOCK)
		printf("FE_HAS_LOCK \n");
	printf("\n");

	return 0;
}

int tune(int frontend_fd, struct tune_p *t)
{
	struct dtv_property p_clear[] = { 
		{ .cmd = DTV_CLEAR }
	};

	struct dtv_properties cmdseq_clear = {
		.num = 1,
		.props = p_clear
	};

	if ((ioctl(frontend_fd, FE_SET_PROPERTY, &cmdseq_clear)) == -1) {
		perror("FE_SET_PROPERTY DTV_CLEAR failed");
		return -1;
	}
	usleep (20000);

	// discard stale QPSK events
	struct dvb_frontend_event ev;
	while (1) {
		if (ioctl(frontend_fd, FE_GET_EVENT, &ev) == -1)
			break;
	}

	struct dtv_property p_tune[] = {
		{ .cmd = DTV_DELIVERY_SYSTEM,	.u.data = t->system },
		{ .cmd = DTV_FREQUENCY,			.u.data = t->freq * 1000 },
		{ .cmd = DTV_VOLTAGE,			.u.data = t->voltage },
		{ .cmd = DTV_SYMBOL_RATE,		.u.data = t->sr * 1000},
		{ .cmd = DTV_TONE,				.u.data = t->tone },
		{ .cmd = DTV_MODULATION,		.u.data = t->modulation },
		{ .cmd = DTV_INNER_FEC,			.u.data = t->fec },
		{ .cmd = DTV_INVERSION,			.u.data = t->inversion },
		{ .cmd = DTV_ROLLOFF,			.u.data = t->rolloff },
		{ .cmd = DTV_BANDWIDTH_HZ,		.u.data = 0 },
		{ .cmd = DTV_PILOT,				.u.data = t->pilot },
		{ .cmd = DTV_DVBS2_MIS_ID,		.u.data = t->mis },
		{ .cmd = DTV_TUNE },
	};
	struct dtv_properties cmdseq_tune = {
		.num = 13,
		.props = p_tune
	};

	printf("\nTuneing specs: \n");
    printf("System:     %s \n", value2name(p_tune[0].u.data, dvb_system));
	printf("Frequency:  %d %s %d \n", abs(p_tune[1].u.data/1000 + t->LO), value2name(p_tune[2].u.data, dvb_voltage) , p_tune[3].u.data / 1000);
	printf("22khz:      %s \n", value2name(p_tune[4].u.data, dvb_tone));
	printf("Modulation: %s \n", value2name(p_tune[5].u.data, dvb_modulation));
	printf("FEC:        %s \n", value2name(p_tune[6].u.data, dvb_fec));
	printf("Inversion:  %s \n", value2name(p_tune[7].u.data, dvb_inversion));
	printf("Rolloff:    %s \n", value2name(p_tune[8].u.data, dvb_rolloff));
	printf("Pilot:      %s \n", value2name(p_tune[10].u.data, dvb_pilot));
	printf("MIS:        %d \n\n", p_tune[11].u.data);

	if (ioctl(frontend_fd, FE_SET_PROPERTY, &cmdseq_tune) == -1) {
		perror("FE_SET_PROPERTY TUNE failed");
		return;
	}
	usleep (200000);

	// wait for zero status indicating start of tunning
	do {
		ioctl(frontend_fd, FE_GET_EVENT, &ev);
	}
	while(ev.status != 0);

	if (ioctl(frontend_fd, FE_GET_EVENT, &ev) == -1) {
		ev.status = 0;
	}

	int i;
	fe_status_t status;
	for ( i = 0; i < 20; i++)
	{
		if (ioctl(frontend_fd, FE_READ_STATUS, &status) == -1) {
			perror("FE_READ_STATUS failed");
			return;
		}

		if (status & FE_HAS_LOCK || status & FE_TIMEDOUT)
			break;
		else
			sleep(1);
	}

	struct dtv_property p[] = {
		{ .cmd = DTV_DELIVERY_SYSTEM },
		{ .cmd = DTV_FREQUENCY },
		{ .cmd = DTV_VOLTAGE },
		{ .cmd = DTV_SYMBOL_RATE },
		{ .cmd = DTV_TONE },
		{ .cmd = DTV_MODULATION },
		{ .cmd = DTV_INNER_FEC },
		{ .cmd = DTV_INVERSION },
		{ .cmd = DTV_ROLLOFF },
		{ .cmd = DTV_BANDWIDTH_HZ },
		{ .cmd = DTV_PILOT },
		{ .cmd = DTV_DVBS2_MIS_ID }
	};

	struct dtv_properties p_status = {
		.num = 12,
		.props = p
	};

	// get the actual parameters from the driver for that channel
	if ((ioctl(frontend_fd, FE_GET_PROPERTY, &p_status)) == -1) {
		perror("FE_GET_PROPERTY failed");
		return -1;
	}

	printf("Tuned specs: \n");
	printf("System:     %s %d \n", value2name(p_status.props[0].u.data, dvb_system), p_status.props[0].u.data);
	printf("Frequency:  %d %s %d \n", abs(p_status.props[1].u.data/1000 + t->LO), value2name(p_status.props[2].u.data, dvb_voltage) , p_status.props[3].u.data / 1000);
	printf("22khz:      %s \n", value2name(p_status.props[4].u.data, dvb_tone));
	printf("Modulation: %s %d \n", value2name(p_status.props[5].u.data, dvb_modulation), p_status.props[5].u.data);
	printf("FEC:        %s %d \n", value2name(p_status.props[6].u.data, dvb_fec), p_status.props[6].u.data);
	printf("Inversion:  %s %d \n", value2name(p_status.props[7].u.data, dvb_inversion), p_status.props[7].u.data);
	printf("Rolloff:    %s %d \n", value2name(p_status.props[8].u.data, dvb_rolloff), p_status.props[8].u.data);
	printf("Pilot:      %s %d \n", value2name(p_status.props[10].u.data, dvb_pilot), p_status.props[10].u.data);
	printf("MIS:        %d \n\n", p_status.props[11].u.data);

	char c;
	do
	{
		check_frontend(frontend_fd);
		c = getch();
//			sleep(1);
//	                if ( kbhit() )
//        	                c = kbgetchar(); /* consume the character */
		switch ( c ) {
			case 'e':
				motor_dir(frontend_fd, 0);
				break;
			case 'w':
				motor_dir(frontend_fd, 1);
				break;
		}
	} while(c != 'q');
	return 0;
}

char *usage =
	"\nusage: tune-s2 12224 V 20000 [options]\n"
	"	-adapter N     : use given adapter (default 0)\n"
	"	-frontend N    : use given frontend (default 0)\n"
	"	-2             : use 22khz tone\n"
	"	-committed N   : use DiSEqC COMMITTED switch position N (1-4)\n"
	"	-uncommitted N : use DiSEqC uncommitted switch position N (1-4)\n"
	"	-usals N.N     : orbital position\n"
	"	-long N.N      : site long\n"
	"	-lat N.N       : site lat\n"
	"	-lnb lnb-type  : STANDARD, UNIVERSAL, DBS, CBAND or \n"
	"	-system        : System DVB-S or DVB-S2\n"
	"	-modulation    : modulation BPSK QPSK 8PSK\n"
	"	-fec           : fec 1/2, 2/3, 3/4, 3/5, 4/5, 5/6, 6/7, 8/9, 9/10, AUTO\n"
	"	-rolloff       : rolloff 35=0.35 25=0.25 20=0.20 0=UNKNOWN\n"
	"	-inversion N   : spectral inversion (OFF / ON / AUTO [default])\n"
	"	-pilot N	   : pilot (OFF / ON / AUTO [default])\n"
	"	-mis N   	   : MIS #\n"
	"	-help          : help\n";

int main(int argc, char *argv[])
{
	if (!argv[1] || strcmp(argv[1], "-help") == 0)
	{
		printf("%s", usage);
		return -1;
	}

	struct lnb_p lnb_DBS = { 11250, 0, 0 };
	struct lnb_p lnb_STANDARD = { 10750, 0, 0 };
	struct lnb_p lnb_10600 = { 10600, 0, 0 };
	struct lnb_p lnb_10745 = { 10745, 0, 0 };
	struct lnb_p lnb_UNIVERSAL = { 9750, 10600, 11700 };
	struct lnb_p lnb_CBAND = { -5150, 0, 0 };
	struct lnb_p lnb_IF = { 0, 0, 0 };
	struct lnb_p lnb = lnb_STANDARD;

	char frontend_devname[80];
	int adapter = 0, frontend = 0;
	int committed	= 0;
	int uncommitted	= 0;

	double site_lat		= 0;
	double site_long	= 0;
	double sat_long		= 0;

	struct tune_p t;
	t.fec		= FEC_AUTO;
	t.system	= SYS_DVBS;
	t.modulation	= QPSK;
	t.rolloff	= ROLLOFF_AUTO;
	t.inversion	= INVERSION_AUTO;
	t.pilot		= PILOT_AUTO;
	t.tone		= SEC_TONE_OFF;
	t.freq		= strtoul(argv[1], NULL, 0);
	t.voltage	= name2value(argv[2], dvb_voltage);
	t.sr		= strtoul(argv[3], NULL, 0);
	t.mis		= -1;

	int a;
	for( a = 4; a < argc; a++ )
	{
		if ( !strcmp(argv[a], "-adapter") )
			adapter = strtoul(argv[a+1], NULL, 0);
		if ( !strcmp(argv[a], "-frontend") )
			frontend = strtoul(argv[a+1], NULL, 0);
		if ( !strcmp(argv[a], "-2") )
			t.tone = SEC_TONE_ON;
		if ( !strcmp(argv[a], "-committed") )
			committed = strtoul(argv[a+1], NULL, 0);
		if ( !strcmp(argv[a], "-uncommitted") )
			uncommitted = strtoul(argv[a+1], NULL, 0);
		if ( !strcmp(argv[a], "-usals") )
			sat_long = strtod(argv[a+1], NULL);
		if ( !strcmp(argv[a], "-long") )
			site_long = strtod(argv[a+1], NULL);
		if ( !strcmp(argv[a], "-lat") )
			site_lat = strtod(argv[a+1], NULL);
		if ( !strcmp(argv[a], "-lnb") )
		{
			if (!strcmp(argv[a+1], "DBS"))		lnb = lnb_DBS;
			if (!strcmp(argv[a+1], "10600"))	lnb = lnb_10600;
			if (!strcmp(argv[a+1], "10745"))	lnb = lnb_10745;
			if (!strcmp(argv[a+1], "STANDARD"))	lnb = lnb_STANDARD;
			if (!strcmp(argv[a+1], "UNIVERSAL"))	lnb = lnb_UNIVERSAL;
			if (!strcmp(argv[a+1], "CBAND"))	lnb = lnb_CBAND;
			if (!strcmp(argv[a+1], "IF"))		lnb = lnb_IF;
		}
		if ( !strcmp(argv[a], "-fec") )
			t.fec = name2value(argv[a+1], dvb_fec);
		if ( !strcmp(argv[a], "-system") )
			t.system = name2value(argv[a+1], dvb_system);
		if ( !strcmp(argv[a], "-modulation") )
			t.modulation = name2value(argv[a+1], dvb_modulation);
		if ( !strcmp(argv[a], "-rolloff") )
			t.rolloff = name2value(argv[a+1], dvb_rolloff);
		if ( !strcmp(argv[a], "-inversion") )
			t.inversion = name2value(argv[a+1], dvb_inversion);
		if ( !strcmp(argv[a], "-pilot") )
			t.pilot = name2value(argv[a+1], dvb_pilot);
		if ( !strcmp(argv[a], "-mis") )
			t.mis = strtoul(argv[a+1], NULL, 0);
		if ( !strcmp(argv[a], "-help") )
		{
			printf("%s", usage);
			return -1;
		}
	}

	printf("LNB: low: %d high: %d switch: %d \n", lnb.low, lnb.high, lnb.threshold);

	snprintf(frontend_devname, 80, "/dev/dvb/adapter%d/frontend%d", adapter, frontend);
	printf("opening: %s\n", frontend_devname);
	int frontend_fd;
	if ((frontend_fd = open(frontend_devname, O_RDWR | O_NONBLOCK)) < 0)
	{
		printf("failed to open '%s': %d %m\n", frontend_devname, errno);
		return -1;
	}

	if (lnb.threshold && lnb.high && t.freq > lnb.threshold)
	{
		printf("HIGH band\n");
		t.tone = SEC_TONE_ON;
		t.LO = lnb.high;
		t.freq = abs(t.freq - abs(t.LO));
	} else {
		printf("LOW band\n");
		t.LO = lnb.low;
		t.freq = abs(t.freq - abs(t.LO));;
	}

	if (sat_long)
		motor_usals(frontend_fd, site_lat, site_long, sat_long);
	if (!t.tone || committed || uncommitted)
		setup_switch (frontend_fd, t.voltage, t.tone, committed, uncommitted);

	tune(frontend_fd, &t);

	printf("Closing frontend ... \n");
	close (frontend_fd);
	return 0;
}
