// select.c ... select scan functions
// part of Multi-attribute Linear-hashed Files
// Manage creating and using Selection objects
// Credit: John Shepherd
// Last modified by Ziyi Shi, Apr 2025

#include "defs.h"
#include "select.h"
#include "reln.h"
#include "tuple.h"
#include "bits.h"
#include "hash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// --------------------------------------------------------------------------
struct SelectionRep {
    Reln    rel;           // Relation info
    Bits    known;         // Hash bits from known attributes
    Bits    unknown;       // Unknown (wildcard) bits
    Page    curpage;       // Current page in scan
    int     is_ovflow;     // 0: main file, 1: ovflow file
    Offset  curtupOffset;  // Offset of current tuple within page data buffer
    Offset  curtupIndex;   // Index of current tuple in the current page
    PageID  curPageId;     // Current main page ID
    PageID  curScanPageId; // Current page ID being scanned
    char   *queryString;   // Original query string
    char  **queryValues;   // Array of query attribute values
    Count   nattrs;        // Number of attributes

    PageID *candidates;    // Array of candidate page IDs computed from known/unknown bits
    Count   ncandidates;   // Number of candidate pages
    Count   currCandidate; // Index of the current candidate page being scanned
};

// --------------------------------------------------------------------------
// Implement pattern matching '?' and '%'

// Params:
//    tupleValue - The attribute value from the tuple
//    queryValue - The query pattern to match against
// Returns:
//    TRUE if the tupleValue matches the queryValue pattern, FALSE otherwise
Bool matchPattern(char *tupleValue, char *queryValue) {
    // Case 1: "?" wildcard matches any tuple value
    if (strcmp(queryValue, "?") == 0) {
        return TRUE;
    }

    // Case 2: Pattern matching with '%' wildcard
    if (strchr(queryValue, '%') != NULL) {
        char *pattern = queryValue;  // Pattern string with possible '%' wildcards
        char *text = tupleValue;      // Text to match against the pattern
        char *p, *t;                  // Pointers for traversing pattern and text
        char *pTag = NULL, *tTag = NULL;  // Tags for backtracking when matching '%'

        p = pattern;
        t = text;

        while (*t) {
            if (*p == '%') {
                // Record the position of '%' for possible backtracking
                pTag = p++;
                tTag = t;

                // Skip consecutive '%' characters
                while (*p == '%') p++;

                // If pattern ends after '%', match succeeds
                if (*p == '\0') return TRUE;
            }
            else if (*p == *t) {
                // Characters match, move both pointers forward
                p++;
                t++;
            }
            else if (pTag) {
                // Mismatch after '%'; backtrack to try matching more characters
                p = pTag + 1;
                t = ++tTag;
            }
            else {
                // No '%' wildcard available and characters don't match
                return FALSE;
            }
        }

        // After text is fully matched, check if the rest of the pattern is only '%'
        while (*p == '%') p++;

        // If pattern is fully consumed, match succeeds
        if (*p == '\0') {
            return TRUE;
        } else {
            return FALSE;
        }
    }

    // Case 3: Exact match (no wildcard)
    if (strcmp(tupleValue, queryValue) == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

// --------------------------------------------------------------------------
// checks whether the tuple matches the query
Bool matchTuple(Selection s, Tuple t) {
    // checks whether the tuple matches the query
    char **tupleValues = malloc(s->nattrs * sizeof(char *));
    tupleVals(t, tupleValues);
    Bool match = TRUE;
    for (int i = 0; i < s->nattrs; i++) {
        if (!matchPattern(tupleValues[i], s->queryValues[i])) {
            match = FALSE;
            break;
        }
    }
    freeVals(tupleValues, s->nattrs);
    return match;
}

// enumerating all possible combinations for the first numBits bits.
// If a bit is unknown, it tries both 0 and 1; if known, it uses the fixed bit.
// According to the given number of bits, all combinations of unknown bits are traversed to generate candidate bucket numbers
PageID * genCandidates(Bits known, Bits unknown, int numBits, int *ncandidates)
{
    // recording unknown bit position
    int unknownCount = 0;
    int pos[32];
    for (int i = 0; i < numBits; i++) {
        if (bitIsSet(unknown, i)) {
            pos[unknownCount++] = i;
        }
    }

    int total = 1 << unknownCount;
    PageID *results = malloc(total * sizeof(PageID));
    assert(results != NULL);

    // go through each combination of unknown bits
    for (int c = 0; c < total; c++) {
        // use an array to store whether each bit is 0 or 1
        int bits[numBits];

        // all bits are initialized and set bits using known
        for (int i = 0; i < numBits; i++) {
            bits[i] = bitIsSet(known, i) ? 1 : 0;
        }

        // setting unknown bit with the combination number c
        for (int i = 0; i < unknownCount; i++) {
            int unknown_bit_value = (c & (1 << i)) ? 1 : 0;
            bits[pos[i]] = unknown_bit_value;
        }

        // Assembled into a PageID value
        PageID candidate = 0;
        for (int i = 0; i < numBits; i++) {
            if (bits[i]) {
                candidate = setBit(candidate, i);
            }
        }

        results[c] = candidate;
    }

    *ncandidates = total;
    return results;
}

// --------------------------------------------------------------------------
// a SelectionRep object is created from the query string and a list of candidate pages is generated
Selection startSelection(Reln r, char *q)
{
    Selection new = malloc(sizeof(struct SelectionRep));
    assert(new != NULL);
    new->rel = r;             // relation being queried
    new->queryString = q;     // original query string
    new->is_ovflow = 0;       // not in overflow pages yet
    new->curtupOffset = 0;    // tuple offset starts at 0
    new->curtupIndex = 0;     // tuple index starts at 0
    new->known = 0;           // known bits initialized to 0
    new->unknown = 0;         // unknown bits initialized to 0
    new->curPageId = 0;       // current page ID
    new->curScanPageId = 0;   // current scanning page ID
    new->nattrs = nattrs(r);  // number of attributes

    // The query string is split by comma and parsed to obtain the query value for each attribute
    new->queryValues = malloc(new->nattrs * sizeof(char *));
    char *temQueryStr = malloc(strlen(q) + 1);
    strcpy(temQueryStr, q);
    char *token;
    int i = 0;
    token = strtok(temQueryStr, ",");
    while (token != NULL && i < new->nattrs) {
        new->queryValues[i] = malloc(strlen(token) + 1);
        assert(new->queryValues[i] != NULL);
        strcpy(new->queryValues[i], token);
        token = strtok(NULL, ",");
        i++;
    }
    // if not enough value is provided, use "?" padding
    while (i < new->nattrs) {
        new->queryValues[i] = malloc(2);
        assert(new->queryValues[i] != NULL);
        strcpy(new->queryValues[i], "?");
        i++;
    }
    free(temQueryStr);

    // a temporary array vals for the hash, if the attribute is ? or %, it is marked with "*" to indicate unknown
    char **vals = malloc(new->nattrs * sizeof(char *));
    for (i = 0; i < new->nattrs; i++) {
        if (strcmp(new->queryValues[i], "?") == 0 || strchr(new->queryValues[i], '%') != NULL) {
            vals[i] = malloc(2);
            assert(vals[i] != NULL);
            strcpy(vals[i], "*");
        } else {
            vals[i] = malloc(strlen(new->queryValues[i]) + 1);
            assert(vals[i] != NULL);
            strcpy(vals[i], new->queryValues[i]);
        }
    }

    // known and unknown are computed using cv
    ChVecItem *cv = chvec(r);
    for (i = 0; i < MAXBITS; i++) {
        int a = cv[i].att;
        int b = cv[i].bit;
        if (strcmp(vals[a], "*") != 0) {
            Bits h = hash_any((unsigned char *)vals[a], strlen(vals[a]));
            if (bitIsSet(h, b)) {
                new->known = setBit(new->known, i);
            }
        } else {
            new->unknown = setBit(new->unknown, i);
        }
    }
    
    for (i = 0; i < new->nattrs; i++) {
        free(vals[i]);
    }
    free(vals);

    // candidate pages are generated using known and unknown bit combination generation
    int depthVal = depth(r);
    PageID sp = splitp(r);
    int countD = 0;
    // d bits are used to generate candidate bucket numbers
    PageID *candD = genCandidates(new->known, new->unknown, depthVal, &countD);

    int countDplus = 0;
    // d + 1 bits are used to generate candidate bucket numbers
    PageID *candDplus = genCandidates(new->known, new->unknown, depthVal + 1, &countDplus);

    // merge the two candidate sets:
    // For d-bit candidates, only those with values >= sp are kept
    // For d+1-bit candidates, only those with values < sp are kept
    int totalCandidates = 0;
    PageID *tempCandidates = malloc((countD + countDplus) * sizeof(PageID));
    assert(tempCandidates != NULL);
    int mask = (1 << depthVal) - 1;
    // d bit candidates are processed
    for (i = 0; i < countD; i++) {
        if (candD[i] >= sp) {
            tempCandidates[totalCandidates++] = candD[i];
        }
    }
    // d+1 bit candidates are processed
    for (i = 0; i < countDplus; i++) {
        if ((candDplus[i] & mask) < sp) {
            tempCandidates[totalCandidates++] = candDplus[i];
        }
    }
    free(candD);
    free(candDplus);

    // copy the candidate pages into the Selection object
    new->candidates = malloc(totalCandidates * sizeof(PageID));
    assert(new->candidates != NULL);
    for (i = 0; i < totalCandidates; i++) {
        new->candidates[i] = tempCandidates[i];
    }
    free(tempCandidates);
    new->ncandidates = totalCandidates;
    new->currCandidate = 0;

    // if a candidate page exists, the first candidate page is loaded
    if (new->ncandidates > 0) {
        new->curPageId = new->candidates[0];
        new->curScanPageId = new->curPageId;
        new->curtupOffset = 0;
        new->curtupIndex = 0;
        new->curpage = getPage(dataFile(r), new->curPageId);
    } else {
        new->curpage = NULL;
    }

    return new;
}

// --------------------------------------------------------------------------
Tuple getNextTuple(Selection s)
{
    // iterate over the set of candidate pages
    while (s->currCandidate < s->ncandidates) {
        if (s->curpage == NULL) {
            s->curPageId = s->candidates[s->currCandidate];
            s->curScanPageId = s->curPageId;
            s->is_ovflow = 0;
            s->curtupIndex = 0;
            s->curtupOffset = 0;
            s->curpage = getPage(dataFile(s->rel), s->curPageId);
        }

        // scan the current candidate page and its overflow chain
        while (s->curpage != NULL) {
            // if there are still unscanned tuples on the current page
            if (s->curtupIndex < pageNTuples(s->curpage)) {
                // use curtupOffset to access the current tuple directly
                char *pageDataPtr = pageData(s->curpage);
                char *tuple = pageDataPtr + s->curtupOffset;
                s->curtupOffset += strlen(tuple) + 1;
                s->curtupIndex++;

                // if a tuple satisfies the query condition, the tuple is returned
                if (matchTuple(s, tuple)) {
                    char *copy = malloc(strlen(tuple) + 1);
                    assert(copy != NULL);
                    strcpy(copy, tuple);
                    return copy;
                }
            } else {
                // all tuples of the current page have been scanned to check for overflow pages
                PageID nextPageId = pageOvflow(s->curpage);
                free(s->curpage);
                if (nextPageId != NO_PAGE) {
                    // overflow page is entered, at which point the state is updated
                    s->is_ovflow = 1;
                    s->curScanPageId = nextPageId;
                    s->curtupIndex = 0;
                    s->curtupOffset = 0;
                    s->curpage = getPage(ovflowFile(s->rel), nextPageId);
                } else {
                    // current candidate page is scanned and the inner loop is exited to load the next candidate page
                    s->curpage = NULL;
                    break;
                }
            }
        }
        // current candidate is processed and moves to the next candidate
        s->currCandidate++;
    }
    return NULL;
}

// --------------------------------------------------------------------------
// closeSelection: release SelectionRep and related resources
void closeSelection(Selection s)
{
    if (s == NULL) return;

    if (s->curpage != NULL) {
        free(s->curpage);
    }

    if (s->queryValues != NULL) {
        for (int i = 0; i < s->nattrs; i++) {
            if (s->queryValues[i] != NULL) {
                free(s->queryValues[i]);
            }
        }
        free(s->queryValues);
    }

    if (s->candidates != NULL) {
        free(s->candidates);
    }

    free(s);
}