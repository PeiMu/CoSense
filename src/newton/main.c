/*
	Authored 2015. Phillip Stanley-Marbell.
	Adapted for Newton in 2017 by Jonathan Lim.

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.

	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.

	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written
		permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <getopt.h>
#include <setjmp.h>
#include <stdint.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"
#include "common-errors.h"
#include "version.h"
#include "noisy-timeStamps.h"
#include "common-timeStamps.h"
#include "common-data-structures.h"

#include "newton-parser.h"
#include "newton-lexer.h"
#include "newton-symbolTable.h"
#include "newton.h"


int
main(int argc, char *argv[])
{
	int		jumpParameter;
	State *		N;


	N = init(kCommonModeDefault);
	
	if (N == NULL)
	{
		fatal(NULL, Emalloc);

		/*	Not reached	*/
		consolePrintBuffers(N);
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		char			tmp;
		char *			ep = &tmp;
		int			optionIndex	= 0, c;
		static struct option	options[]	=
		{
			{"verbose",		required_argument,	0,	'v'},
			{"help",		no_argument,		0,	'h'},
			{"version",		no_argument,		0,	'V'},
			{"dot",			required_argument,	0,	'd'},
			{"smt",			required_argument,	0,	'S'},
			{"bytecode",		required_argument,	0,	'b'},
			{"trace",		no_argument,		0,	't'},
			{"statistics",		no_argument,		0,	's'},
			{"optimize",		required_argument,	0,	'O'},
			{"dmatrixannote",	no_argument,		0,	'm'},
			{"pigroups",		no_argument,		0,	'p'},
			{"kernelrowcanon",	no_argument,		0,	'c'},
			{"pigroupsort",		no_argument,		0,	'r'},
			{"pigroupdedup",	no_argument,		0,	'e'},
			{"pikernelprinter",	no_argument,		0,	'P'},
			{"pigrouptoast",	no_argument,		0,	'a'},
			{"codegen",		required_argument,	0,	'g'},
			{"latex",		no_argument,		0,	'x'},
			{"RTLcodegen",		required_argument,	0,	'l'},
			{0,			0,			0,	0}
		};

		c = getopt_long(argc, argv, "v:hVd:S:b:stO:mpcl:rePapg:x", options, &optionIndex);

		if (c == -1)
		{
			break;
		}

		switch (c)
		{
			case 0:
			{
				/*
				 *	Not sure what the expected behavior for getopt_long is here...
				 */
				break;
			}

			case 'h':
			{
				usage(N);
				consolePrintBuffers(N);
				exit(EXIT_SUCCESS);

				/*	Not reached 	*/
				break;
			}

			case 'V':
			{
				version(N);
				consolePrintBuffers(N);
				exit(EXIT_SUCCESS);

				/*	Not reached 	*/
				break;
			}

			case 'd':
			{
				N->irBackends |= kNewtonIrBackendDot;

				/*
				 *	TODO: Rather than accepting the raw enum value as integer,
				 *	accept string and compare to table of options
				 */
				uint64_t tmpInt = strtoul(optarg, &ep, 0);
				if (*ep == '\0')
				{
					N->dotDetailLevel = tmpInt;
				}
				else
				{
					usage(N);
					consolePrintBuffers(N);
					exit(EXIT_FAILURE);
				}

				break;
			}

			case 'S':
			{
				N->irBackends |= kNewtonIrBackendSmt;
				N->outputSmtFilePath = optarg;

				break;
			}

			case 't':
			{
				N->mode |= kCommonModeCallTracing;
				N->mode |= kCommonModeCallStatistics;

				break;
			}

			case 's':
			{
				N->mode |= kCommonModeCallStatistics;

				break;
			}

			case 'v':
			{
				uint64_t tmpInt = strtoul(optarg, &ep, 0);
				if (*ep == '\0')
				{
					/*
					 *	The verbosity bitmaps are:
					 *
					 *		...
					 *		kCommonVerbosityDebugLexer
					 *		kCommonVerbosityDebugParser
					 *		kCommonVerbosityDebugAST
					 *		...
					 *
					 *	(See common/common-data-structures.h)
					 *
					 */

					N->verbosityLevel = tmpInt;
				}
				else
				{
					usage(N);
					consolePrintBuffers(N);
					exit(EXIT_FAILURE);
				}

				break;
			}

			case 'O':
			{
				/*
				 *	TODO: define a structured way for which passes depend on which
				 */

				/*
				 *	Implies the following (basic) passes:
				 */
				N->irPasses |= kNewtonIrPassDimensionalMatrixKernelRowCanonicalization;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroupSorted;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroupsWeedOutDuplicates;

				uint64_t tmpInt = strtoul(optarg, &ep, 0);
				if (*ep == '\0')
				{
					N->optimizationLevel = tmpInt;
				}
				else
				{
					usage(N);
					consolePrintBuffers(N);
					exit(EXIT_FAILURE);
				}

				break;
			}

			case 'm':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;

				break;
			}

			case 'p':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroups;

				break;
			}

			case 'c':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroups;
				N->irPasses |= kNewtonIrPassDimensionalMatrixKernelRowCanonicalization;

				break;
			}

			case 'r':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroups;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroupSorted;

				break;
			}

			case 'e':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroups;
				N->irPasses |= kNewtonIrPassDimensionalMatrixKernelRowCanonicalization;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroupSorted;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroupsWeedOutDuplicates;

				break;
			}

			case 'P':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroups;
				N->irPasses |= kNewtonIrPassDimensionalMatrixKernelPrinter;

				break;
			}

			case 'a':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroups;
				N->irPasses |= kNewtonIrPassDimensionalMatrixConvertToList;

				break;
			}

			case 'g':
			{
				N->irBackends |= kNewtonIrBackendC;
				N->outputCFilePath = optarg;

				break;
			}

			case 'x':
			{
				N->irPasses |= kNewtonIrPassDimensionalMatrixAnnotation;
				N->irPasses |= kNewtonIrPassDimensionalMatrixPiGroups;
				N->irPasses |= kNewtonIrPassDimensionalMatrixKernelPrinter;
				N->irBackends |= kNewtonIrBackendLatex;

				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\documentclass{article}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\usepackage{amsmath}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\usepackage{amssymb}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\usepackage[a0paper, portrait]{geometry}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\usepackage{color}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\definecolor{DarkSlateGray}{rgb}{0.1843,0.3098,0.3098}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\definecolor{DeepSkyBlue}{rgb}{0,0.7490,1}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\definecolor{DarkGreen}{rgb}{0,0.3922,0}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\begin{document}\n");
				flexprint(N->Fe, N->Fm, N->Fpmathjax, "\\tiny\n");
				break;
			}

			case 'l':
			{
				N->irBackends |= kNewtonIrBackendRTL;
				N->outputRTLFilePath = optarg;

				break;
			}

			case '?':
			{
				/*
				 *	getopt_long() should have already printed an error message.
				 */
				usage(N);
				consolePrintBuffers(N);
				exit(EXIT_FAILURE);

				break;
			}

			default:
			{
				usage(N);
				consolePrintBuffers(N);
				exit(EXIT_FAILURE);
			}
		}
	}

	if (N->mode & kCommonModeCallStatistics)
	{
		timestampsInit(N);
	}

	if (optind < argc)
	{
		while (optind < argc)
		{
			jumpParameter = setjmp(N->jmpbuf);
			if (!jumpParameter)
			{
				processNewtonFile(N, argv[optind++]);
			}
			else
			{
				/*	Return again after longjmp	*/
				flexprint(N->Fe, N->Fm, N->Fperr, "Processing Newton file failed (returned again after longjmp()): Source file number (passed in jumpParameter) was %d\n", jumpParameter);
			}
		}
	}
	else
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "\n%s\n", Enoinput);
		usage(N);
		consolePrintBuffers(N);
		exit(EXIT_FAILURE);
	}

	if (!(N->mode & kCommonModeCGI))
	{
		consolePrintBuffers(N);
	}

	return 0;
}



void
version(State *  N)
{
	flexprint(N->Fe, N->Fm, N->Fperr, "\nNewton version %s.\n\n", kNewtonVersion);
}


void
usage(State *  N)
{
	version(N);
	flexprint(N->Fe, N->Fm, N->Fperr,	"Usage:    newton-<uname>-%s\n"
						"                [ (--help, -h)                                               \n"
						"                | (--version, --V)                                           \n"
						"                | (--verbose <level>, -v <level>)                            \n"
						"                | (--dot <level>, -d <level>)                                \n"
						"                | (--smt <path to output file>, -S <path to output file>)    \n"
						"                | (--bytecode <output file name>, -b <output file name>)     \n"
						"                | (--optimize <level>, -O <level>)                           \n"
						"                | (--dmatrixannote, -m)                                      \n"
						"                | (--pigroups, -p)                                           \n"
						"                | (--kernelrowcanon, -c)                                     \n"
						"                | (--pigroupsort, -r)                                        \n"
						"                | (--pigroupdedup, -e)                                       \n"
						"                | (--pikernelprinter, -P)                                    \n"
						"                | (--pigrouptoast, -a)                                       \n"
						"                | (--codegen <path to output file>, -g <path to output file>)\n"
						"                | (--RTLcodegen <path to output file>, -r <path to output file>)\n"
						"                | (--trace, -t)                                              \n"
						"                | (--statistics, -s) ]                                       \n"
						"                | (--latex, -x) ]                                            \n"
						"                                                                             \n"
						"              <filenames>\n\n", kNewtonL10N);
}
