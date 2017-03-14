#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"
#include "version.h"
#include "common-timeStamps.h"
#include "common-errors.h"
#include "data-structures.h"
#include "common-irHelpers.h"
#include "noisy-parser.h"
#include "newton-parser-expression.h"
#include "newton-parser.h"
#include "common-lexers-helpers.h"
#include "newton-lexer.h"
#include "newton-symbolTable.h"
#include "common-firstAndFollow.h"


extern char *		gNewtonAstNodeStrings[];
extern int		gNewtonFirsts[kNoisyIrNodeTypeMax][kNoisyIrNodeTypeMax];

extern void		fatal(State *  N, const char *  msg);
extern void		error(State *  N, const char *  msg);

/*
 * ParseNumericExpression is only used to parse expressions of numbers and dimensionless constants inside exponents.
 * It was inconvenient just to use ParseQuantityExpression for the following reason.
 * Although we do not want to evaluate expressions at compile time, evaluating
 * expressions inside exponents is necessary for compile time dimensional checking.
 * e.g.) The expression, mass ** 2, yields two "mass" dimensions in numeratorDimensions.
 * If we use ParseQuantityExpression, then sometimes not all the terms and factors have
 * numeric values known. To distinguish the two cases, we can either pass in a flag to quantity parsing methods
 * or just use ParseNumericExpression. 
 * e.g.) Pi == 3.14 but mass might not have a numeric value.
 *
 * We use kNewtonIrNodeType_PquantityTerm and kNewtonIrNodeType_PquantityFactor because
 * constant physics structs are essentially quantityFactors.
 */
IrNode *
newtonParseNumericExpression(State * N, Scope * currentScope)
{
    IrNode * leftTerm;
    IrNode * rightTerm;

    if (inFirst(N, kNewtonIrNodeType_PquantityTerm, gNewtonFirsts))
    {
        leftTerm = newtonParseNumericTerm(N, currentScope);

        while (inFirst(N, kNewtonIrNodeType_PlowPrecedenceBinaryOp, gNewtonFirsts))
        {
            IrNode * binOp = newtonParseLowPrecedenceBinaryOp(N, currentScope);
            addLeaf(N, leftTerm, binOp);
            
            rightTerm = newtonParseNumericTerm(N, currentScope);
            addLeafWithChainingSeq(N, leftTerm, rightTerm);

            if (binOp->type == kNewtonIrNodeType_Tplus) 
            {
                leftTerm->value += rightTerm->value;
            }
            else if (binOp->type == kNewtonIrNodeType_Tminus)
            {
                leftTerm->value -= rightTerm->value;
            }
        }
    }
    else
    {
        fatal(N, Esanity);
    }
    
    return leftTerm;
}

IrNode *
newtonParseNumericTerm(State * N, Scope * currentScope)
{
    IrNode *   intermediate = genIrNode(N,   kNewtonIrNodeType_PquantityTerm,
                        NULL /* left child */,
                        NULL /* right child */,
                        lexPeek(N, 1)->sourceInfo /* source info */);
    intermediate->value = 1;
    if (inFirst(N, kNewtonIrNodeType_PunaryOp, gNewtonFirsts))
    {
        addLeaf(N, intermediate, newtonParseUnaryOp(N, currentScope));
        intermediate->value *= -1;
    }
    
    IrNode * leftFactor = newtonParseNumericFactor(N, currentScope);
    intermediate->value *= leftFactor->value;

    addLeafWithChainingSeq(N, intermediate, leftFactor);
    
    while (inFirst(N, kNewtonIrNodeType_PmidPrecedenceBinaryOp, gNewtonFirsts))
    {
        IrNode * binOp = newtonParseMidPrecedenceBinaryOp(N, currentScope);
        addLeafWithChainingSeq(N, intermediate, binOp);
        
        IrNode * rightFactor = newtonParseNumericFactor(N, currentScope);
        addLeafWithChainingSeq(N, intermediate, rightFactor);
        
        if (binOp->type == kNewtonIrNodeType_Tmul) 
        {
            intermediate->value *= rightFactor->value;
        }
        else if (binOp->type == kNewtonIrNodeType_Tdiv)
        {
            intermediate->value /= rightFactor->value;
        }
    }

    return intermediate;
}

IrNode *
newtonParseNumericFactor(State * N, Scope * currentScope)
{
    IrNode *   node;

    if (peekCheck(N, 1, kNewtonIrNodeType_Tidentifier))
    {
        node = newtonParseIdentifierUsageTerminal(N, kNewtonIrNodeType_Tidentifier, currentScope);
        assert(node->physics->isConstant);
    }
    else if (peekCheck(N, 1, kNewtonIrNodeType_Tnumber))
    {
        node = newtonParseTerminal(N, kNewtonIrNodeType_Tnumber, currentScope);
        assert(node->value != 0); /* TODO remove later */
    }
    else if (peekCheck(N, 1, kNewtonIrNodeType_TleftParen))
    {
        newtonParseTerminal(N, kNewtonIrNodeType_TleftParen, currentScope);
        node = newtonParseNumericExpression(N, currentScope);
        newtonParseTerminal(N, kNewtonIrNodeType_TrightParen, currentScope);
    }
    else
    {
        fatal(N, "newtonParseQuantityFactor: missed a case in factor\n");
    }

    if (inFirst(N, kNewtonIrNodeType_PhighPrecedenceBinaryOp, gNewtonFirsts))
    {
        addLeaf(N, node, newtonParseHighPrecedenceBinaryOp(N, currentScope));

        /* exponents are automatically just one integer unless wrapped in parens */
        IrNode * exponentExpression = peekCheck(N, 1, kNewtonIrNodeType_TleftParen) ? 
            newtonParseNumericExpression(N, currentScope) : 
            newtonParseInteger(N, currentScope);
        addLeaf(N, node, exponentExpression);

        /* 0 ** 0 in mathematics is indeterminate */
        assert(node->value != 0 || exponentExpression->value != 0);
        node->value = pow(node->value, exponentExpression->value);
    }

    return node;
}

IrNode *
newtonParseQuantityExpression(State * N, Scope * currentScope)
{
    IrNode *   expression = genIrNode(N,   kNewtonIrNodeType_PquantityExpression,
                                                  NULL /* left child */,
                                                  NULL /* right child */,
                                                  lexPeek(N, 1)->sourceInfo /* source info */);

    expression->physics = (Physics *) calloc(1, sizeof(Physics));
    expression->physics->numeratorPrimeProduct = 1;
    expression->physics->denominatorPrimeProduct = 1;

    N->currentParameterNumber = 0;

    IrNode * leftTerm;
    IrNode * rightTerm;

    if (inFirst(N, kNewtonIrNodeType_PquantityTerm, gNewtonFirsts))
      {
        leftTerm = newtonParseQuantityTerm(N, currentScope);
        expression->value = leftTerm->value;
        expression->physics = leftTerm->physics;
        addLeaf(N, expression, leftTerm);


        while (inFirst(N, kNewtonIrNodeType_PlowPrecedenceBinaryOp, gNewtonFirsts))
          {
            addLeafWithChainingSeq(N, expression, newtonParseLowPrecedenceBinaryOp(N, currentScope));

            rightTerm = newtonParseQuantityTerm(N, currentScope);
            addLeafWithChainingSeq(N, leftTerm, rightTerm);
            expression->value += rightTerm->value;

            // compare LHS and RHS prime numbers and make sure they're equal
            assert(leftTerm->physics->numeratorPrimeProduct == rightTerm->physics->numeratorPrimeProduct);
            assert(leftTerm->physics->denominatorPrimeProduct == rightTerm->physics->denominatorPrimeProduct);
          }
      }
    else
      {
        fatal(N, Esanity);
      }

    return expression;
}


IrNode *
newtonParseQuantityTerm(State * N, Scope * currentScope)
{
    IrNode *   intermediate = genIrNode(N,   kNewtonIrNodeType_PquantityTerm,
                        NULL /* left child */,
                        NULL /* right child */,
                        lexPeek(N, 1)->sourceInfo /* source info */);

    intermediate->physics = (Physics *) calloc(1, sizeof(Physics));
    intermediate->physics->numeratorPrimeProduct = 1;
    intermediate->physics->denominatorPrimeProduct = 1;
    intermediate->value = 1;

    bool isUnary = false;

    if (inFirst(N, kNewtonIrNodeType_PunaryOp, gNewtonFirsts))
    {
        addLeaf(N, intermediate, newtonParseUnaryOp(N, currentScope));
        isUnary = true;
    }

    bool hasNumberInTerm = false;
    bool isPhysics /*not a number*/ = peekCheck(N, 1, kNewtonIrNodeType_Tidentifier);
    IrNode * leftFactor = newtonParseQuantityFactor(N, currentScope);
    addLeafWithChainingSeq(N, intermediate, leftFactor);
    hasNumberInTerm = hasNumberInTerm || leftFactor->physics == NULL || leftFactor->physics->isConstant;
    if (hasNumberInTerm)
      {
        intermediate->value = isUnary ? leftFactor->value * -1 : leftFactor->value;
      }

    int numVectorsInTerm = 0;

    if (isPhysics)
    {
        if (leftFactor->physics->numeratorDimensions)
            newtonPhysicsCopyNumeratorDimensions(N, intermediate->physics, leftFactor->physics);
        if (leftFactor->physics->denominatorDimensions)
            newtonPhysicsCopyDenominatorDimensions(N, intermediate->physics, leftFactor->physics);

        /*
         * If either LHS or RHS is a vector (not both), then the resultant is a vector
         */
        if (leftFactor->physics->isVector)
        {
            intermediate->physics->isVector = true;
            numVectorsInTerm++;
        }
    }

    IrNode * rightFactor;

    while (inFirst(N, kNewtonIrNodeType_PmidPrecedenceBinaryOp, gNewtonFirsts))
    {
        IrNode * binOp = newtonParseMidPrecedenceBinaryOp(N, currentScope);
        addLeafWithChainingSeq(N, intermediate, binOp);

        bool isPhysics = peekCheck(N, 1, kNewtonIrNodeType_Tidentifier);
        rightFactor = newtonParseQuantityFactor(N, currentScope);
        addLeafWithChainingSeq(N, intermediate, rightFactor);
        hasNumberInTerm = hasNumberInTerm || leftFactor->physics == NULL || leftFactor->physics->isConstant;

        if (hasNumberInTerm)
          {
            if (binOp->type == kNewtonIrNodeType_Tmul)
              {
                intermediate->value = rightFactor->value == 0 ? intermediate->value : intermediate->value * rightFactor->value;
              }
            else if (binOp->type == kNewtonIrNodeType_Tdiv)
              {
                intermediate->value = rightFactor->value == 0 ? intermediate->value : intermediate->value / rightFactor->value;
              }
          }

        // TODO double check this logic when I'm more awake
        if (isPhysics && rightFactor->physics->isVector)
        {
            intermediate->physics->isVector = true;
            numVectorsInTerm++;

            /*
             * Cannot perform multiply or divide operations on two vectors
             * e.g.) vector * scalar * scalar / vector is illegal because
             * it boils down to vector / vector which is illegal
             */
            assert(numVectorsInTerm < 2);
        }


        if (isPhysics && binOp->type == kNewtonIrNodeType_Tmul) 
        {
            if (rightFactor->physics->numeratorDimensions)
                newtonPhysicsCopyNumeratorDimensions(N, intermediate->physics, rightFactor->physics);
            if (rightFactor->physics->denominatorDimensions)
                newtonPhysicsCopyDenominatorDimensions(N, intermediate->physics, rightFactor->physics);
        }
        else if (isPhysics && binOp->type == kNewtonIrNodeType_Tdiv)
        {
            if (rightFactor->physics->numeratorDimensions)
                newtonPhysicsCopyNumeratorToDenominatorDimensions(N, intermediate->physics, rightFactor->physics);
            if (rightFactor->physics->denominatorDimensions)
                newtonPhysicsCopyDenominatorToNumeratorDimensions(N, intermediate->physics, rightFactor->physics);
        }
    }

    if (! hasNumberInTerm)
      intermediate->value = 0;

    return intermediate;
}

IrNode *
newtonParseQuantityFactor(State * N, Scope * currentScope)
{
    IrNode *   factor;

    if (peekCheck(N, 1, kNewtonIrNodeType_Tidentifier))
    {
        factor = newtonParseIdentifierUsageTerminal(N, kNewtonIrNodeType_Tidentifier, currentScope);
        factor->physics = deepCopyPhysicsNode(factor->physics);
        factor->value = factor->physics->value;

        assert(factor->tokenString != NULL);

        /* Is a matchable parameter corresponding the invariant parameter */
        if (!newtonIsDimensionless(factor->physics) && !factor->physics->isConstant && newtonDimensionTableDimensionForIdentifier(N, N->newtonIrTopScope, factor->tokenString) == NULL)
        {
          factor->parameterNumber = N->currentParameterNumber++;
        }
    }
    else if (peekCheck(N, 1, kNewtonIrNodeType_Tnumber))
    {
        factor = newtonParseTerminal(N, kNewtonIrNodeType_Tnumber, currentScope);
    }
    // TODO implement these later
    // else if (inFirst(N, kNewtonIrNodeType_PtimeOp, gNewtonFirsts))
    // {
    //     factor = newtonParseTimeOp(N, currentScope);
    // }
    // else if (inFirst(N, kNewtonIrNodeType_PvectorOp, gNewtonFirsts) && peekCheck(N, 2, kNewtonIrNodeType_TleftParen) && peekCheck(N, 4, kNewtonIrNodeType_Tcomma))
    // {
	  // 	factor = newtonParseVectorOp(N, currentScope);
    // }
    else if (peekCheck(N, 1, kNewtonIrNodeType_TleftParen))
    {
        newtonParseTerminal(N, kNewtonIrNodeType_TleftParen, currentScope);
        factor = newtonParseQuantityExpression(N, currentScope);
        newtonParseTerminal(N, kNewtonIrNodeType_TrightParen, currentScope);
    }
    else
    {
        fatal(N, "newtonParseQuantityFactor: missed a case in factor\n");
    }

    /*
     * e.g.) (acceleration * mass) ** (3 + 5)
     */
    if (inFirst(N, kNewtonIrNodeType_PhighPrecedenceBinaryOp, gNewtonFirsts))
    {
        addLeaf(N, factor, newtonParseHighPrecedenceBinaryOp(N, currentScope));
        IrNode * exponentialExpression = newtonParseExponentialExpression(N, currentScope, factor);
        addLeafWithChainingSeq(N, factor, exponentialExpression);
        if (factor->value != 0)
          factor->value = pow(factor->value, exponentialExpression->value);
    }

    return factor;
}

IrNode *
newtonParseExponentialExpression(State * N, Scope * currentScope, IrNode * baseNode)
{
    /* exponents are automatically just one integer unless wrapped in parens */
    IrNode * exponent = peekCheck(N, 1, kNewtonIrNodeType_TleftParen) ? 
        newtonParseNumericExpression(N, currentScope) : 
        newtonParseInteger(N, currentScope);
    Physics * newExponentBase = shallowCopyPhysicsNode(baseNode->physics);

    if (exponent->value == 0)
    {
        /* any dimension raised to zero power has dimensions removed */
        newExponentBase->value = 1;
        baseNode->physics = newExponentBase;

        return exponent;
    }
    else
    {
      newExponentBase->value = pow(newExponentBase->value, exponent->value);
    }

    /*
     * This copying is necessary because we don't want to append the same node multiple times
     * to the numerator or denominator linked list
     */
    Physics* copy = deepCopyPhysicsNode(baseNode->physics);

    if (baseNode->physics->numberOfNumerators > 0)
    {
        /* If the base is a Physics quantity, the exponent must be an integer */
        assert(exponent->value == (int) exponent->value); 

        /* e.g.) mass ** -2 : mass is copied to denominator, not numerator */
        if (exponent->value < 0)
        {
            for (int power = 0; power > exponent->value; power--)
            {
                newtonPhysicsCopyNumeratorToDenominatorDimensions(N, newExponentBase, copy);
                copy = deepCopyPhysicsNode(copy);
            }
        }
        else
        {
            for (int power = 0; power < exponent->value; power++)
            {
                newtonPhysicsCopyNumeratorDimensions(N, newExponentBase, copy);
                copy = deepCopyPhysicsNode(copy);
            }
        }
    }
    
    if (baseNode->physics->numberOfDenominators > 0)
    {
        assert(exponent->value == (int) exponent->value); 
        
        if (exponent->value < 0)
        {
            for (int power = 0; power > exponent->value; power--)
            {
                newtonPhysicsCopyDenominatorToNumeratorDimensions(N, newExponentBase, copy);
                copy = deepCopyPhysicsNode(copy);
            }
        }
        else
        {
            for (int power = 0; power < exponent->value; power++)
            {
                newtonPhysicsCopyDenominatorDimensions(N, newExponentBase, copy);
                copy = deepCopyPhysicsNode(copy);
            }
        }
    }

    baseNode->physics = newExponentBase;

    return exponent;
}

IrNode *
newtonParseVectorOp(State *  N, Scope * currentScope)
{
    IrNode *   intermediate = genIrNode(N,   kNewtonIrNodeType_PvectorOp,
                        NULL /* left child */,
                        NULL /* right child */,
                        lexPeek(N, 1)->sourceInfo /* source info */);

    intermediate->physics = (Physics *) calloc(1, sizeof(Physics));
    intermediate->physics->numeratorPrimeProduct = 1;
    intermediate->physics->denominatorPrimeProduct = 1;

    bool addAngleToDenominator = false;

    if (peekCheck(N, 1, kNewtonIrNodeType_Tdot))
    {
        addLeaf(N, intermediate, newtonParseTerminal(N, kNewtonIrNodeType_Tdot, currentScope));
    } 
    else if (peekCheck(N, 1, kNewtonIrNodeType_Tcross))
    {
        addLeaf(N, intermediate, newtonParseTerminal(N, kNewtonIrNodeType_Tcross, currentScope));
        addAngleToDenominator = true;
    } 
    else 
    {
        fatal(N, "newtonParseVectorOp: op is not dot or cross\n");
    }
    
    newtonParseTerminal(N, kNewtonIrNodeType_TleftParen, currentScope);
    
    IrNode * left;
    left = newtonParseQuantityExpression(N, currentScope);
    addLeafWithChainingSeq(N, intermediate, left);

    newtonPhysicsCopyNumeratorDimensions(N, intermediate->physics, left->physics);
    newtonPhysicsCopyDenominatorDimensions(N, intermediate->physics, left->physics);
    
    newtonParseTerminal(N, kNewtonIrNodeType_Tcomma, currentScope);
    
    IrNode * right;
    right = newtonParseQuantityExpression(N, currentScope);
    addLeafWithChainingSeq(N, intermediate, right);

    assert(left->physics->isVector && right->physics->isVector);

    newtonPhysicsCopyNumeratorDimensions(N, intermediate->physics, right->physics);
    newtonPhysicsCopyDenominatorDimensions(N, intermediate->physics, right->physics);

    if (addAngleToDenominator) 
    {
        Dimension* angle = newtonDimensionTableDimensionForIdentifier(N, currentScope, "rad");
        newtonPhysicsAddDenominatorDimension(N, intermediate->physics, angle);
    } 
    
    newtonParseTerminal(N, kNewtonIrNodeType_TrightParen, currentScope);

    return intermediate;
}


IrNode *
newtonParseLowPrecedenceBinaryOp(State *  N, Scope * currentScope)
{
    IrNode *   n;

    if (peekCheck(N, 1, kNewtonIrNodeType_Tplus))
    {
        n = newtonParseTerminal(N, kNewtonIrNodeType_Tplus, currentScope);
    }
    else if (peekCheck(N, 1, kNewtonIrNodeType_Tminus))
    {
        n = newtonParseTerminal(N, kNewtonIrNodeType_Tminus, currentScope);
    }
    else
    {
        // noisyParserErrorRecovery(N, kNewtonIrNodeType_PlowPrecedenceBinaryOp);
        return NULL;
    }

    return n;
}

IrNode *
newtonParseUnaryOp(State *  N, Scope * currentScope)
{
    IrNode *   n = NULL;

    if (peekCheck(N, 1, kNewtonIrNodeType_Tminus))
    {
        n = newtonParseTerminal(N, kNewtonIrNodeType_Tminus, currentScope);
    }
    else
    {
        fatal(N, "newton-parser-expression.c: newtonParseUnaryOp: did not detect minus as unary op\n");
    }

    return n;
}

IrNode *
newtonParseTimeOp(State * N, Scope * currentScope)
{
	IrNode *	node = genIrNode(
        N,
        kNewtonIrNodeType_PtimeOp,
		NULL /* left child */,
		NULL /* right child */,
		lexPeek(N, 1)->sourceInfo /* source info */
    );
    
    IrNodeType type;
    if ((type = lexPeek(N, 1)->type) == kNewtonIrNodeType_Tintegral || 
         type == kNewtonIrNodeType_Tderivative)
    {
		addLeaf(N, node, newtonParseTerminal(N, type, currentScope));
    }
    else
    {
        fatal(N, "newton-parser-expression.c:newtonParseTimeOp: did not detect derivative or integral\n");
    }

    while ((type = lexPeek(N, 1)->type) == kNewtonIrNodeType_Tintegral || 
            type == kNewtonIrNodeType_Tderivative)
    {
		addLeafWithChainingSeq(N, node, newtonParseTerminal(N, type, currentScope));
        addLeafWithChainingSeq(N, node, newtonParseQuantityExpression(N, currentScope));
    }


    return node;
}

IrNode *
newtonParseCompareOp(State * N, Scope * currentScope)
{
    
    IrNodeType type;
    if ((type = lexPeek(N, 1)->type) == kNewtonIrNodeType_Tlt || 
         type == kNewtonIrNodeType_Tle ||
         type == kNewtonIrNodeType_Tge ||
         type == kNewtonIrNodeType_Tgt ||
         type == kNewtonIrNodeType_Tproportionality ||
         type == kNewtonIrNodeType_Tequivalent
       )
    {
		return newtonParseTerminal(N, type, currentScope);
    }
    else
    {
        fatal(N, "newton-parser-expression.c:newtonParseCompareOp op is not a compare op");
    }
}

IrNode *
newtonParseHighPrecedenceBinaryOp(State * N, Scope * currentScope)
{
	IrNode *	node = genIrNode(
        N,
        kNewtonIrNodeType_PhighPrecedenceBinaryOp,
		NULL /* left child */,
		NULL /* right child */,
		lexPeek(N, 1)->sourceInfo /* source info */
    );
    
    if (peekCheck(N, 1, kNewtonIrNodeType_Texponent))
    {
		addLeaf(N, node, newtonParseTerminal(N, kNewtonIrNodeType_Texponent, currentScope));
    }
    else
    {
        fatal(N, "newton-parser-expression.c:newtonParseHighPrecedenceBinaryOp: no exponent token\n");
    }
    return node;
}

IrNode *
newtonParseMidPrecedenceBinaryOp(State *  N, Scope * currentScope)
{
    IrNode *   n;

    if (peekCheck(N, 1, kNewtonIrNodeType_Tmul))
    {
        n = newtonParseTerminal(N, kNewtonIrNodeType_Tmul, currentScope);
    }
    else if (peekCheck(N, 1, kNewtonIrNodeType_Tdiv))
    {
        n = newtonParseTerminal(N, kNewtonIrNodeType_Tdiv, currentScope);
    }
    else
    {
        fatal(N, "newton-parser-expression.c: newtonParseMidPrecedenceBinaryOp not a mid precedence binop\n");
    }

    return n;
}

IrNode *
newtonParseInteger(State * N, Scope * currentScope)
{
	IrNode *	node = genIrNode(N,	kNewtonIrNodeType_Pinteger,
						NULL /* left child */,
						NULL /* right child */,
						lexPeek(N, 1)->sourceInfo /* source info */);
    
    if (inFirst(N, kNewtonIrNodeType_PunaryOp, gNewtonFirsts))
    {
        addLeaf(N, node, newtonParseUnaryOp(N, currentScope));
        node->value = -1;
    }

    IrNode * number = newtonParseTerminal(N, kNewtonIrNodeType_Tnumber, currentScope);
    addLeaf(N, node, number);
    node->value = node->value == -1 ? node->value * number->value : number->value;
        
    assert(node->value != 0); // TODO remove this assertion later bc value MIGHT be 0

    return node;
}
