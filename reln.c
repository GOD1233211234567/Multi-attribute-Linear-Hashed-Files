// reln.c ... functions on Relations
// part of Multi-attribute Linear-hashed Files
// Credit: John Shepherd
// Last modified by Ziyi Shi, Apr 2025

#include "defs.h"
#include "reln.h"
#include "page.h"
#include "tuple.h"
#include "chvec.h"
#include "bits.h"
#include "hash.h"

#define HEADERSIZE (3*sizeof(Count)+sizeof(Offset))

struct RelnRep {
	Count  nattrs; // number of attributes
	Count  depth;  // depth of main data file
	Offset sp;     // split pointer
    Count  npages; // number of main data pages
    Count  ntups;  // total number of tuples
	ChVec  cv;     // choice vector
	char   mode;   // open for read/write
	FILE  *info;   // handle on info file
	FILE  *data;   // handle on data file
	FILE  *ovflow; // handle on ovflow file
};

// create a new relation (three files)

Status newRelation(char *name, Count nattrs, Count npages, Count d, char *cv)
{
    char fname[MAXFILENAME];
	Reln r = malloc(sizeof(struct RelnRep));
	r->nattrs = nattrs; r->depth = d; r->sp = 0;
	r->npages = npages; r->ntups = 0; r->mode = 'w';
	assert(r != NULL);
	if (parseChVec(r, cv, r->cv) != OK) return ~OK;
	sprintf(fname,"%s.info",name);
	r->info = fopen(fname,"w");
	assert(r->info != NULL);
	sprintf(fname,"%s.data",name);
	r->data = fopen(fname,"w");
	assert(r->data != NULL);
	sprintf(fname,"%s.ovflow",name);
	r->ovflow = fopen(fname,"w");
	assert(r->ovflow != NULL);
	int i;
	for (i = 0; i < npages; i++) addPage(r->data);
	closeRelation(r);
	return 0;
}

// check whether a relation already exists

Bool existsRelation(char *name)
{
	char fname[MAXFILENAME];
	sprintf(fname,"%s.info",name);
	FILE *f = fopen(fname,"r");
	if (f == NULL)
		return FALSE;
	else {
		fclose(f);
		return TRUE;
	}
}

// set up a relation descriptor from relation name

Reln openRelation(char *name, char *mode)
{
	Reln r;
	r = malloc(sizeof(struct RelnRep));
	assert(r != NULL);
	char fname[MAXFILENAME];
	sprintf(fname,"%s.info",name);
	r->info = fopen(fname,mode);
	assert(r->info != NULL);
	sprintf(fname,"%s.data",name);
	r->data = fopen(fname,mode);
	assert(r->data != NULL);
	sprintf(fname,"%s.ovflow",name);
	r->ovflow = fopen(fname,mode);
	assert(r->ovflow != NULL);
	// Naughty: assumes Count and Offset are the same size
	int n = fread(r, sizeof(Count), 5, r->info);
	assert(n == 5);
	n = fread(r->cv, sizeof(ChVecItem), MAXCHVEC, r->info);
	assert(n == MAXCHVEC);
	r->mode = (mode[0] == 'w' || mode[1] =='+') ? 'w' : 'r';
	return r;
}

// release files and descriptor for an open relation
// copy latest information to .info file

void closeRelation(Reln r)
{
	// make sure updated global data is put in info
	// Naughty: assumes Count and Offset are the same size
	if (r->mode == 'w') {
		fseek(r->info, 0, SEEK_SET);
		// write out core relation info (#attr,#pages,d,sp)
		int n = fwrite(r, sizeof(Count), 5, r->info);
		assert(n == 5);
		// write out choice vector
		n = fwrite(r->cv, sizeof(ChVecItem), MAXCHVEC, r->info);
		assert(n == MAXCHVEC);
	}
	fclose(r->info);
	fclose(r->data);
	fclose(r->ovflow);
	free(r);
}

PageID insertTupleIntoPageChain(Reln r, PageID p, Tuple t) {
    Page pg = getPage(r->data,p);
    if (addToPage(pg,t) == OK) {
        putPage(r->data,p,pg);
        return p;
    }
    // primary data page full
    if (pageOvflow(pg) == NO_PAGE) {
        // add first overflow page in chain
        PageID newp = addPage(r->ovflow);
        pageSetOvflow(pg,newp);
        putPage(r->data,p,pg);
        Page newpg = getPage(r->ovflow,newp);
        // can't add to a new page; we have a problem
        if (addToPage(newpg,t) != OK) return NO_PAGE;
        putPage(r->ovflow,newp,newpg);
        return p;
    }
    else {
        // scan overflow chain until we find space
        // worst case: add new ovflow page at end of chain
        Page ovpg, prevpg = NULL;
        PageID ovp, prevp = NO_PAGE;
        ovp = pageOvflow(pg);
        free(pg);
        while (ovp != NO_PAGE) {
            ovpg = getPage(r->ovflow, ovp);
            if (addToPage(ovpg,t) != OK) {
                if (prevpg != NULL) free(prevpg);
                prevp = ovp; prevpg = ovpg;
                ovp = pageOvflow(ovpg);
            }
            else {
                if (prevpg != NULL) free(prevpg);
                putPage(r->ovflow,ovp,ovpg);
                return p;
            }
        }
        // all overflow pages are full; add another to chain
        // at this point, there *must* be a prevpg
        assert(prevpg != NULL);
        // make new ovflow page
        PageID newp = addPage(r->ovflow);
        // insert tuple into new page
        Page newpg = getPage(r->ovflow,newp);
        if (addToPage(newpg,t) != OK) return NO_PAGE;
        putPage(r->ovflow,newp,newpg);
        // link to existing overflow chain
        pageSetOvflow(prevpg,newp);
        putPage(r->ovflow,prevp,prevpg);
        return p;
    }
}

// insert a new tuple into a relation
// returns index of bucket where inserted
// - index always refers to a primary data page
// - the actual insertion page may be either a data page or an overflow page
// returns NO_PAGE if insert fails completely
// TODO: include splitting and file expansion
PageID addToRelation(Reln r, Tuple t)
{
    Bits h, p;
    //char buf[MAXBITS+5]; //*** for debug
    h = tupleHash(r,t);  // Returns Bits - tuple's hash value
    if (r->depth == 0)
        p = 0;
    else {
        p = getLower(h, r->depth);  // Returns Bits - extracts lower bits from hash value
        if (p < r->sp) p = getLower(h, r->depth+1);  // Returns Bits
    }
    // bitsString(h,buf); printf("hash = %s\n",buf); //*** for debug
    // bitsString(p,buf); printf("page = %s\n",buf); //*** for debug

    p = insertTupleIntoPageChain(r, p, t);  // Returns PageID - page where tuple was inserted
    if (p != NO_PAGE) {
        r->ntups++;
        Count c = 1024 / (10 * nattrs(r));  // nattrs() returns Count - number of attributes
        if (r->ntups > 0 && r->ntups % c == 0) {
            // Split the old bucket
            PageID newPageId = r->sp + (1 << r->depth); // new page ID sp + 2^d
            PageID oldPageId = r->sp; // old page id

            // Creating a new bucket
            Page newPageObj = newPage();
            r->npages++;
            putPage(r->data, newPageId, newPageObj);

            // Get the old bucket and its overflow chain
            Page oldPageObj = getPage(r->data, oldPageId);
            PageID ovflowID = pageOvflow(oldPageObj);

            // Collect all the tuples
            int maxTuples = pageNTuples(oldPageObj);
            PageID currentOvp_for_count = ovflowID;
            while (currentOvp_for_count != NO_PAGE) {
                Page ovPage = getPage(r->ovflow, currentOvp_for_count);
                maxTuples += pageNTuples(ovPage);
                currentOvp_for_count = pageOvflow(ovPage);
                free(ovPage);
            }

            char **tuples = malloc(sizeof(char*) * maxTuples);
            assert(tuples != NULL);
            int ntuples = 0;

            // Collect tuples from the master data page
            char *c = pageData(oldPageObj);
            for (int i = 0; i < pageNTuples(oldPageObj); i++) {
                tuples[ntuples++] = copyString(c);
                c += strlen(c) + 1;
            }

            PageID currentOvp = ovflowID;
            while (currentOvp != NO_PAGE) {
                Page ovPage = getPage(r->ovflow, currentOvp);
                c = pageData(ovPage);
                for (int i = 0; i < pageNTuples(ovPage); i++) {
                    tuples[ntuples++] = copyString(c);
                    c += strlen(c) + 1;
                }
                PageID nextOvp = pageOvflow(ovPage);
                free(ovPage);
                currentOvp = nextOvp;
            }


            PageID firstOverflowID = pageOvflow(oldPageObj);
            PageID cur_OverflowPage_ID = firstOverflowID;
            Page emptyPageObj = newPage();
            pageSetOvflow(emptyPageObj, firstOverflowID);
            putPage(r->data, oldPageId, emptyPageObj);


            while (cur_OverflowPage_ID != NO_PAGE) {
                Page currOvPage = getPage(r->ovflow, cur_OverflowPage_ID);
                assert(currOvPage != NULL);

                PageID nextOvID = pageOvflow(currOvPage);

                free(currOvPage);

                Page newOvPage = newPage();

                pageSetOvflow(newOvPage, NO_PAGE);

                putPage(r->ovflow, cur_OverflowPage_ID, newOvPage);

                cur_OverflowPage_ID = nextOvID;
            }

            // Reallocate all tuples
            for (int i = 0; i < ntuples; i++) {
                Tuple t = tuples[i];
                Bits h = tupleHash(r, t);
                // Use an extra bit to determine the bucket number
                Bits newPid = getLower(h, r->depth + 1);
                insertTupleIntoPageChain(r, newPid, t);
                free(t);
            }

            free(tuples);


            r->sp++;
            if (r->sp == (1 << r->depth)) {
                r->depth++;
                r->sp = 0;
            }
        }
    }
    return p;
}

// external interfaces for Reln data

FILE *dataFile(Reln r) { return r->data; }
FILE *ovflowFile(Reln r) { return r->ovflow; }
Count nattrs(Reln r) { return r->nattrs; }
Count npages(Reln r) { return r->npages; }
Count ntuples(Reln r) { return r->ntups; }
Count depth(Reln r)  { return r->depth; }
Count splitp(Reln r) { return r->sp; }
ChVecItem *chvec(Reln r)  { return r->cv; }


// displays info about open Reln

void relationStats(Reln r)
{
	printf("Global Info:\n");
	printf("#attrs:%d  #pages:%d  #tuples:%d  d:%d  sp:%d\n",
	       r->nattrs, r->npages, r->ntups, r->depth, r->sp);
	printf("Choice vector\n");
	printChVec(r->cv);
	printf("Bucket Info:\n");
	printf("%-4s %s\n","#","Info on pages in bucket");
	printf("%-4s %s\n","","(pageID,#tuples,freebytes,ovflow)");
	for (Offset pid = 0; pid < r->npages; pid++) {
		printf("[%2d]  ",pid);
		Page p = getPage(r->data, pid);
		Count ntups = pageNTuples(p);
		Count space = pageFreeSpace(p);
		Offset ovid = pageOvflow(p);
		printf("(d%d,%d,%d,%d)",pid,ntups,space,ovid);
		free(p);
		while (ovid != NO_PAGE) {
			Offset curid = ovid;
			p = getPage(r->ovflow, ovid);
			ntups = pageNTuples(p);
			space = pageFreeSpace(p);
			ovid = pageOvflow(p);
			printf(" -> (ov%d,%d,%d,%d)",curid,ntups,space,ovid);
			free(p);
		}
		putchar('\n');
	}
}
