/*
	Authored 2017. Jonathan Lim

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
#include <assert.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"
#include "common-errors.h"
#include "version.h"
#include "newton-timeStamps.h"
#include "common-timeStamps.h"
#include "common-data-structures.h"

#include "newton-parser.h"
#include "newton-lexer.h"
#include "newton-symbolTable.h"
#include "newton.h"
#include "newton-irPass-dotBackend.h"
#include "newton-irPass-smtBackend.h"
#include "newton-dimension-pass.h"

extern char *	gNewtonAstNodeStrings[kNoisyIrNodeTypeMax];

static State *
processNewtonFileDimensionPass(char * filename);


void
processNewtonFile(State *  N, char *  filename)
{
	/*
	 *	Tokenize input, then parse it and build AST + symbol table.
	 */
	newtonLexInit(N, filename);

	/*
	 *	Create a top-level scope, then parse.
	 */
	N->newtonIrTopScope = newtonSymbolTableAllocScope(N);

	State *     N_dim = processNewtonFileDimensionPass(filename);
	N->newtonIrTopScope->firstDimension = N_dim->newtonIrTopScope->firstDimension;

	assert(N->newtonIrTopScope->firstDimension != NULL);

	N->newtonIrRoot = newtonParse(N, N->newtonIrTopScope);

	/*
	 *	Dot backend.
	 */
	if (N->irBackends & kNoisyIrBackendDot)
	{
		fprintf(stdout, "%s\n", irPassDotBackend(N, N->newtonIrTopScope, N->newtonIrRoot, gNewtonAstNodeStrings));
 	}

	/*
	*	Smt backend
	*/
	if (N->irBackends & kNewtonIrBackendSmt)
	{
		irPassSmtBackend(N);
	}
}

static State*
processNewtonFileDimensionPass(char * filename)
{
	State *	    N = init(kNoisyModeDefault);
	newtonLexInit(N, filename);

	N->newtonIrTopScope = newtonSymbolTableAllocScope(N);
	newtonDimensionPassParse(N, N->newtonIrTopScope);

	return N;
}