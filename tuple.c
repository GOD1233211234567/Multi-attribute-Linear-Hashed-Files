// tuple.c ... functions on tuples
// part of Multi-attribute Linear-hashed Files
// Credt: John Shepherd
// Last modified by Ziyi Shi, Apr 2024

#include "defs.h"
#include "tuple.h"
#include "reln.h"
#include "hash.h"
#include "chvec.h"
#include "bits.h"
#include "util.h"

// return number of bytes/chars in a tuple

int tupLength(Tuple t)
{
	return strlen(t);
}

// reads/parses next tuple in input

Tuple readTuple(Reln r, FILE *in)
{
	char line[MAXTUPLEN];
	if (fgets(line, MAXTUPLEN-1, in) == NULL)
		return NULL;
	line[strlen(line)-1] = '\0';
	// count fields
	// cheap'n'nasty parsing
	char *c; int nf = 1;
	for (c = line; *c != '\0'; c++)
		if (*c == ',') nf++;
	// invalid tuple
	if (nf != nattrs(r)) return NULL;
	return copyString(line); // needs to be free'd sometime
}

// extract values into an array of strings

void tupleVals(Tuple t, char **vals)
{
	char *c = t, *c0 = t;
	int i = 0;
	for (;;) {
		while (*c != ',' && *c != '\0') c++;
		if (*c == '\0') {
			// end of tuple; add last field to vals
			vals[i++] = copyString(c0);
			break;
		}
		else {
			// end of next field; add to vals
			*c = '\0';
			vals[i++] = copyString(c0);
			*c = ',';
			c++; c0 = c;
		}
	}
}

// release memory used for separate attribute values

void freeVals(char **vals, int nattrs)
{
	int i;
    // release memory used for each attribute
	for (i = 0; i < nattrs; i++) free(vals[i]);
    // release memory used for pointer array
    free(vals);
}

// hash a tuple using the choice vector
// TODO: actually use the choice vector to make the hash

// Params:
//    r - Relation descriptor
//    t - Tuple string
// Returns:
//    Bits - The computed hash value for the tuple
Bits tupleHash(Reln r, Tuple t)
{
    char buf[MAXBITS+5];

    Count nvals = nattrs(r);  // Get the number of attributes in the relation

    char **vals = malloc(nvals * sizeof(char *));
    assert(vals != NULL);

    tupleVals(t, vals);

    // Array to hold the hash value of each attribute
    Bits h[nvals + 1];

    // Get the relation's choice vector
    ChVecItem *cv = chvec(r);

    Bits hash = 0;  // Initialize the final combined hash value

    int i, a, b;

    // Hash each attribute value separately
    for (i = 0; i < nvals; i++) {
        h[i] = hash_any((unsigned char *)vals[i], strlen(vals[i]));
    }

    // Construct the final hash using the choice vector
    for (i = 0; i < MAXBITS; i++) {
        a = cv[i].att;  // Attribute index to pick from
        b = cv[i].bit;  // Bit position within the attribute's hash

        if (bitIsSet(h[a], b)) {
            hash = setBit(hash, i);  // Set the corresponding bit in the final hash
        }
    }

    // print the resulting hash value in string format
    bitsString(hash, buf); 
    // printf("hash(%s) = %s\n", t, buf);

    freeVals(vals, nvals);

    return hash;
}

// compare two tuples (allowing for "unknown" values)
// TODO: actually compare values
Bool tupleMatch(Reln r, Tuple pt, Tuple t)
{
	Count na = nattrs(r);
	char **ptv = malloc(na*sizeof(char *));
	tupleVals(pt, ptv);
	char **v = malloc(na*sizeof(char *));
	tupleVals(t, v);
	Bool match = TRUE;
	freeVals(ptv,na); freeVals(v,na);
	return match;
}

// puts printable version of tuple in user-supplied buffer

void tupleString(Tuple t, char *buf)
{
	strcpy(buf,t);
}

// release memory used for tuple
void freeTuple(Tuple t)
{
    if (t != NULL) {
        free(t);
    }
}
