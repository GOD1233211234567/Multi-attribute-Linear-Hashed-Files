// project.c ... project scan functions
// part of Multi-attribute Linear-hashed Files
// Manage creating and using Projection objects
// Last modified by Ziyi Shi, Apr 2025

#include <stdbool.h>
#include "defs.h"
#include "project.h"
#include "reln.h"
#include "tuple.h"
#include "util.h"



// A suggestion ... you can change however you like

struct ProjectionRep {
    Reln    rel;       // relation info
    Count   nattrs;    // number of attributes
    int     *attrList; // used for attribute index lists
    int     projCount; // number of projected attributes
    Bool    allAttrs;  // whether to project all attributes
};

// Parameters:
//    r       - Relation descriptor
//    attrstr - Comma-separated attribute list or "*" for all attributes
// Returns:
//    Pointer to a new Projection object
Projection startProjection(Reln r, char *attrstr)
{
    Projection new = malloc(sizeof(struct ProjectionRep));
    assert(new != NULL);

    new->rel = r;
    new->nattrs = nattrs(r);
    new->allAttrs = FALSE;

    // Checks for "*" on all properties
    if (strcmp(attrstr, "*") == 0) {
        new->allAttrs = TRUE;
        new->projCount = new->nattrs;
        new->attrList = malloc(new->nattrs * sizeof(int));
        for (int i = 0; i < new->nattrs; i++) {
            new->attrList[i] = i;
        }
        return new;
    }

    // count the number of projected attributes
    char *tmp = malloc(strlen(attrstr) + 1);
    assert(tmp != NULL);
    strcpy(tmp, attrstr);
    char *token;
    int count = 0;
    token = strtok(tmp, ",");
    while (token != NULL) {
        count++;
        token = strtok(NULL, ",");
    }
    free(tmp);

    // Allocate memory and parse the property index
    new->projCount = count;
    new->attrList = malloc(count * sizeof(int));

    tmp = malloc(strlen(attrstr) + 1);
    assert(tmp != NULL);
    strcpy(tmp, attrstr);

    token = strtok(tmp, ",");
    for (int i = 0; i < count && token != NULL; i++) {

        new->attrList[i] = atoi(token) - 1;
        token = strtok(NULL, ",");
    }
    free(tmp);

    return new;
}

void projectTuple(Projection p, Tuple t, char *buf)
{
    if (p->allAttrs) {
        // projecting all attributes
        strcpy(buf, t);
        return;
    }

    // extract the attribute values in the tuple
    char **values = malloc(p->nattrs * sizeof(char *));
    tupleVals(t, values);

    // constructing projection results
    // initialized to an empty string
    buf[0] = '\0';
    for (int i = 0; i < p->projCount; i++) {
        int attrIndex = p->attrList[i];

        // adding attribute values
        if (i > 0) strcat(buf, ",");
        strcat(buf, values[attrIndex]);
    }

    freeVals(values, p->nattrs);
}

void closeProjection(Projection p)
{
    if (p != NULL) {
        if (p->attrList != NULL) {
            free(p->attrList);
        }
        free(p);
    }
}