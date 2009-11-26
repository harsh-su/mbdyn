/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2009
 *
 * Pierangelo Masarati	<masarati@aero.polimi.it>
 * Paolo Mantegazza	<mantegazza@aero.polimi.it>
 *
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 * via La Masa, 34 - 20156 Milano, Italy
 * http://www.aero.polimi.it
 *
 * Changing this copyright notice is forbidden.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>

volatile sig_atomic_t keep_going = 1;

static void
sh(int signum)
{
	keep_going = 0;
	signal(signum, SIG_DFL);
}

static int
check_flag(const char *flag, int sleeptime)
{
	while (1) {
		FILE *f;
		char c = ' ';
		int rc;

		f = fopen(flag, "r");
		if (f == NULL && errno == ENOENT) {
			printf("testedge: file \"%s\" created\n", flag);
			return 1;
		}

		rc = fread((void *)&c, 1, 1, f);
		fclose(f);
		if (rc == 1 && c == '3') {
			printf("testedge: got %c from file \"%s\"\n",
				c, flag);
			break;
		}

		printf("testedge: got %c from file \"%s\"; sleeping %d s\n",
			c, flag, sleeptime);
		sleep(sleeptime);
	}

	return 0;
}

static int
put_flag(const char *flag, int cmd)
{
	FILE *f = fopen(flag, "w");

	if (f == NULL) {
		fprintf(stderr, "unable to open flag file \"%s\"\n", flag);
		exit(EXIT_FAILURE);
	}

	fprintf(f, "%d", cmd);
	fclose(f);

	return 0;
}

void
usage(void)
{
	fprintf(stderr,
		"usage: testedge [options]\n"
		"\t-c [random:]<c>\t\tnumber of iterations\n"
		"\t-m [flag|data]=<file>\tmodal file names (set both)\n"
		"\t-M <modes>\tmodes number\n"
		"\t-r [flag|data]=<file>\trigid-body file names (set both)\n"
		"\t-s <sleeptime>\t\tsleep time between tries\n" );
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char *rflag = NULL;
	char *rdata = NULL;
	char *mflag = NULL;
	char *mdata = NULL;
	int sleeptime = 1;
	int iters = 1;
	int iters_random = 0;
	unsigned steps;
	int modes = 5;

	while (1) {
		int opt = getopt(argc, argv, "c:m:M:r:s:");

		if (opt == EOF) {
			break;
		}

		switch (opt) {
		case 'c':
			if (strncasecmp(optarg, "random:", sizeof("random:") -1) == 0) {
				iters_random = 1;
				iters = atoi(&optarg[sizeof("random:") -1]);

			} else {
				iters = atoi(optarg);
				printf("iterations: %d\n", iters);
			}
			if (iters < 1) {
				fprintf(stderr, "testedge: "
					"invalid sleep time %s\n",
					optarg);
				usage();
			}
			break;

		case 'm':
			if (strncasecmp(optarg, "flag=", sizeof("flag=") - 1) == 0) {
				mflag = &optarg[sizeof("flag=") - 1];

			} else if (strncasecmp(optarg, "data=", sizeof("data=") - 1) == 0) {
				mdata = &optarg[sizeof("data=") - 1];

			} else {
				fprintf(stderr, "testedge: "
					"unknown modal file \"%s\"\n",
					optarg);
				usage();
			}
			break;

		case 'M':
			modes = atoi(optarg);
			if (modes <= 0) {
				fprintf(stderr, "testedge: "
					"invalid mode number %s\n",
					optarg);
				usage();
			}
			break;

		case 'r':
			if (strncasecmp(optarg, "flag=", sizeof("flag=") - 1) == 0) {
				rflag = &optarg[sizeof("flag=") - 1];

			} else if (strncasecmp(optarg, "data=", sizeof("data=") - 1) == 0) {
				rdata = &optarg[sizeof("data=") - 1];

			} else {
				fprintf(stderr, "testedge: "
					"unknown rigid file \"%s\"\n",
					optarg);
				usage();
			}
			break;

		case 's':
			sleeptime = atoi(optarg);
			if (sleeptime < 0) {
				fprintf(stderr, "testedge: "
					"invalid iters %s\n",
					optarg);
				usage();
			}
			break;

		default:
			usage();
		}
	}

	if (mflag == NULL && mdata != NULL) {
		fprintf(stderr, "testedge: "
			"need modal flag file "
			"along with modal data file \"%s\"\n",
			mdata);
		usage();
	}

	if (mflag != NULL && mdata == NULL) {
		fprintf(stderr, "testedge: "
			"need modal data file "
			"along with modal flag file \"%s\"\n",
			mflag);
		usage();
	}

	if (rflag == NULL && rdata != NULL) {
		fprintf(stderr, "testedge: "
			"need rigid flag file "
			"along with rigid data file \"%s\"\n",
			rdata);
		usage();
	}

	if (rflag != NULL && rdata == NULL) {
		fprintf(stderr, "testedge: "
			"need rigid data file "
			"along with rigid flag file \"%s\"\n",
			rflag);
		usage();
	}

	if (mflag == NULL && rflag == NULL) {
		fprintf(stderr, "testedge: "
			"need at least rigid or modal files\n");
		usage();
	}

	signal(SIGTERM, sh);
	signal(SIGINT, sh);

	for (steps = 0; keep_going > 0; steps++) {
		int iter;
		int niters;

		if (iters_random) {
			niters = rand() % iters + 1;
			printf("    iterations within this iter: %d\n", niters);

		} else {
			niters = iters;
		}

		for (iter = 0; iter < niters; iter++) {
			FILE *f = NULL;
			int cmd = 2;

			if (iter == niters - 1) {
				cmd = 4;
			}

			/* rigid */
			if (rflag != NULL) {
				check_flag(rflag, sleeptime);

				f = fopen(rdata, "w");
				if (f == NULL) {
					fprintf(stderr, "unable to open rigid data file \"%s\"\n", rflag);
					exit(EXIT_FAILURE);
				}

				fprintf(f,
					"* rigid-body forces and moments\n"
					"body_forces,R,1,6,0\n"
					"0.1 0.2 0.3 0.4 0.5 0.6\n");
				fclose(f);

				put_flag(rflag, cmd);
			}

			/* modal */
			if (mflag != NULL) {
				int i;

				check_flag(mflag, sleeptime);

				f = fopen(mdata, "w");
				if (f == NULL) {
					fprintf(stderr, "unable to open modal data file \"%s\"\n", rflag);
					exit(EXIT_FAILURE);
				}

				fprintf(f,
					"* modal forces\n"
					"modal_force_flow,R,%d,1,0\n",
					modes);
				for (i = 0; i < modes; i++) {
					fprintf(f, "%e ", ((double)i)/10.0 );
				}
				fprintf(f, "\n");
				fclose(f);

				put_flag(mflag, cmd);
			}
		}
	}

	if (rflag != NULL) {
		put_flag(rflag, 5);
	}

	if (mflag != NULL) {
		put_flag(mflag, 5);
	}

	return 0;
}