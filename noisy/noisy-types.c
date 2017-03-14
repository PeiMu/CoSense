/*
	Authored 2015. Phillip Stanley-Marbell.

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

#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"
#include "common-errors.h"
#include "version.h"
#include "common-timeStamps.h"
#include "data-structures.h"


extern const char	gNoisyTypeNodeSignatures[];
extern const char	gNoisyAstNodeStrings[];


IrNode *
noisyTypeValidateIrSubtree(State *  N, IrNode *  subtree)
{
	TimeStampTraceMacro(kNoisyTimeStampKeyTypeValidateIrSubtree);

	return NULL;
}


bool
noisyTypeEqualsSubtreeTypes(State *  N, IrNode *  subtreeA, IrNode *  subtreeB)
{
	TimeStampTraceMacro(kNoisyTimeStampKeyTypeEqualsSubtreeTypes);

	return false;
}


char *
noisyTypeMakeTypeSignature(State *  N, IrNode *  subtree)
{
	TimeStampTraceMacro(kNoisyTimeStampKeyTypeMakeTypeSignature);

	char *	signature;
	char *	leftSignature;
	char *	rightSignature;


	/*
	 *	Type string is a list of chars representing nodes seen on a
	 *	post-order walk of tree rooted at n.  The possible node types
	 *	that will be seen and their signature chars are defined in m.m.
	 */
	if (subtree == NULL)
	{
		return strdup("");
	}

	char s = gNoisyTypeNodeSignatures[subtree->type];
	if (s == 0)
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "%s, node type is %d (%s)\n", EcannotFindTypeSignatureForNodeType, subtree->type, gNoisyAstNodeStrings[subtree->type]);
		fatal(N, Esanity);
	}

	leftSignature	= noisyTypeMakeTypeSignature(N, subtree->irLeftChild);
	rightSignature	= noisyTypeMakeTypeSignature(N, subtree->irRightChild);

	signature = calloc(strlen(leftSignature) + strlen(rightSignature) + 2, sizeof(char));
	if (signature == NULL)
	{
		fatal(N, Emalloc);
	}

	strcpy(signature, leftSignature);
	strcpy(&signature[strlen(leftSignature)], rightSignature);
	signature[strlen(leftSignature) + strlen(rightSignature)] = s;
	signature[strlen(leftSignature) + strlen(rightSignature) + 1] = '\0';

	free(leftSignature);
	free(rightSignature);


	return signature;
}
