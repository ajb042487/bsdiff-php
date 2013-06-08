/*
 +----------------------------------------------------------------------+
 | PHP Version 5                                                        |
 +----------------------------------------------------------------------+
 | Copyright (c) 1997-2013 The PHP Group                                |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Author: rainkide@gmail.com                                           |
 +----------------------------------------------------------------------+
 */

/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
//
//source file from : http://www.daemonology.net/bsdiff
//
/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_bsdiff.h"
#include "zend_exceptions.h"

/* If you declare any globals in php_bsdiff.h uncomment this:
 ZEND_DECLARE_MODULE_GLOBALS(bsdiff)
 */

/* True global resources - no need for thread safety here */
static int le_bsdiff;

#define BSDIFF_MIN(x,y) (((x)<(y)) ? (x) : (y))

static void split(off_t *I,off_t *V,off_t start,off_t len,off_t h)
{
	off_t i,j,k,x,tmp,jj,kk;

	if(len<16) {
		for(k=start;k<start+len;k+=j) {
			j=1;x=V[I[k]+h];
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {
					x=V[I[k+i]+h];
					j=0;
				};
				if(V[I[k+i]+h]==x) {
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
					j++;
				};
			};
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			if(j==1) I[k]=-1;
		};
		return;
	};

	x=V[I[start+len/2]+h];
	jj=0;kk=0;
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;kk+=jj;

	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	if(jj>start) split(I,V,start,jj-start,h);

	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	if(jj==kk-1) I[jj]=-1;

	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(off_t *I,off_t *V,u_char *old,off_t oldsize)
{
	off_t buckets[256];
	off_t i,h,len;

	for(i=0;i<256;i++) buckets[i]=0;
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	I[0]=oldsize;
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				len-=I[i];
				i-=I[i];
			} else {
				if(len) I[i-len]=-len;
				len=V[I[i]]+1-i;
				split(I,V,i,len,h);
				i+=len;
				len=0;
			};
		};
		if(len) I[i-len]=-len;
	};

	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static off_t matchlen(u_char *old,off_t oldsize,u_char *new,off_t newsize)
{
	off_t i;

	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(old[i]!=new[i]) break;

	return i;
}

static off_t search(off_t *I,u_char *old,off_t oldsize,
		u_char *new,off_t newsize,off_t st,off_t en,off_t *pos)
{
	off_t x,y;

	if(en-st<2) {
		x=matchlen(old+I[st],oldsize-I[st],new,newsize);
		y=matchlen(old+I[en],oldsize-I[en],new,newsize);

		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	x=st+(en-st)/2;
	if(memcmp(old+I[x],new,BSDIFF_MIN(oldsize-I[x],newsize))<0) {
		return search(I,old,oldsize,new,newsize,x,en,pos);
	} else {
		return search(I,old,oldsize,new,newsize,st,x,pos);
	};
}

static off_t offtin(u_char *buf)
{
	off_t y;

	y=buf[7]&0x7F;
	y=y*256;y+=buf[6];
	y=y*256;y+=buf[5];
	y=y*256;y+=buf[4];
	y=y*256;y+=buf[3];
	y=y*256;y+=buf[2];
	y=y*256;y+=buf[1];
	y=y*256;y+=buf[0];

	if(buf[7]&0x80) y=-y;

	return y;
}

static void offtout(off_t x,u_char *buf)
{
	off_t y;

	if(x<0) y=-x; else y=x;

		buf[0]=y%256;y-=buf[0];
	y=y/256;buf[1]=y%256;y-=buf[1];
	y=y/256;buf[2]=y%256;y-=buf[2];
	y=y/256;buf[3]=y%256;y-=buf[3];
	y=y/256;buf[4]=y%256;y-=buf[4];
	y=y/256;buf[5]=y%256;y-=buf[5];
	y=y/256;buf[6]=y%256;y-=buf[6];
	y=y/256;buf[7]=y%256;

	if(x<0) buf[7]|=0x80;
}

PHP_FUNCTION( bsdiff_diff) {

	int fd;
	u_char *old_file, *new_file, *diff_file;
	u_char *old, *new;
	off_t oldsize, newsize;
	off_t *I, *V;
	off_t scan, pos, len;
	off_t lastscan, lastpos, lastoffset;
	off_t oldscore, scsc;
	off_t s, Sf, lenf, Sb, lenb;
	off_t overlap, Ss, lens;
	off_t i;
	off_t dblen, eblen;
	u_char *db, *eb;
	u_char buf[8];
	u_char header[32];
	FILE * pf;
	BZFILE * pfbz2;
	int bz2err;

	zval * self = getThis();
	zval * z_old_file, *z_new_file, *z_diff_file;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", &z_old_file, &z_new_file, &z_diff_file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	old_file = Z_STRVAL_P(z_old_file);
	new_file = Z_STRVAL_P(z_new_file);
	diff_file = Z_STRVAL_P(z_diff_file);

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(old_file,O_RDONLY,0))<0) ||
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=malloc(oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,old,oldsize)!=oldsize) ||
		(close(fd)==-1)) {
		zend_error(E_ERROR, "Internal error:%s", old_file);
		RETURN_FALSE;
	}

	if(((I=malloc((oldsize+1)*sizeof(off_t)))==NULL) ||
		((V=malloc((oldsize+1)*sizeof(off_t)))==NULL)) {
		zend_error(E_ERROR, "Internal error", 0 TSRMLS_CC);
		RETURN_FALSE;
	}
	qsufsort(I, V, old, oldsize);
	free(V);

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(new_file,O_RDONLY,0))<0) ||
		((newsize=lseek(fd,0,SEEK_END))==-1) ||
		((new=malloc(newsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,new,newsize)!=newsize) ||
		(close(fd)==-1)) {
		zend_error(E_ERROR, "Internal error:%s", new_file);
		RETURN_FALSE;
	}

	if(((db=malloc(newsize+1))==NULL) ||
		((eb=malloc(newsize+1))==NULL)) {
		zend_error(E_ERROR, "Internal error", 0 TSRMLS_CC);
		RETURN_FALSE;
	}
	dblen=0;
	eblen=0;
	/* Create the patch file */
	if ((pf = fopen(diff_file, "w+")) == NULL) {
		zend_error(E_ERROR, "Internal error: can not create file(%s)", diff_file);
		RETURN_FALSE;
	}

	/* Header is
		0	8	 "BSDIFF40"
		8	8	length of bzip2ed ctrl block
		16	8	length of bzip2ed diff block
		24	8	length of new file */
	/* File is
		0	32	Header
		32	??	Bzip2ed ctrl block
		??	??	Bzip2ed diff block
		??	??	Bzip2ed extra block */
	memcpy(header,"BSDIFF40",8);
	offtout(0, header + 8);
	offtout(0, header + 16);
	offtout(newsize, header + 24);
	if (fwrite(header, 32, 1, pf) != 1) {
		zend_error(E_ERROR, "Internal error: fwrite(%s)", diff_file);
		RETURN_FALSE;
	}

	/* Compute the differences, writing ctrl as we go */
	if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL) {
		zend_error(E_ERROR, "BZ2_bzWriteOpen, bz2err = %d", bz2err);
		RETURN_FALSE;
	}
	scan=0;len=0;
	lastscan=0;lastpos=0;lastoffset=0;
	while(scan<newsize) {
		oldscore=0;

		for(scsc=scan+=len;scan<newsize;scan++) {
			len=search(I,old,oldsize,new+scan,newsize-scan,
					0,oldsize,&pos);

			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<oldsize) &&
				(old[scsc+lastoffset] == new[scsc]))
				oldscore++;

			if(((len==oldscore) && (len!=0)) ||
				(len>oldscore+8)) break;

			if((scan+lastoffset<oldsize) &&
				(old[scan+lastoffset] == new[scan]))
				oldscore--;
		};

		if((len!=oldscore) || (scan==newsize)) {
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<oldsize);) {
				if(old[lastpos+i]==new[lastscan+i]) s++;
				i++;
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			lenb=0;
			if(scan<newsize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(old[pos-i]==new[scan-i]) s++;
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);
				s=0;Ss=0;lens=0;
				for(i=0;i<overlap;i++) {
					if(new[lastscan+lenf-overlap+i]==
					   old[lastpos+lenf-overlap+i]) s++;
					if(new[scan-lenb+i]==
					   old[pos-lenb+i]) s--;
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				lenf+=lens-overlap;
				lenb-=lens;
			};

			for(i=0;i<lenf;i++)
				db[dblen+i]=new[lastscan+i]-old[lastpos+i];
			for(i=0;i<(scan-lenb)-(lastscan+lenf);i++)
				eb[eblen+i]=new[lastscan+lenf+i];

			dblen+=lenf;
			eblen+=(scan-lenb)-(lastscan+lenf);

			offtout(lenf,buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK) {
				zend_error(E_ERROR, "BZ2_bzWrite, bz2err = %d", bz2err);
				RETURN_FALSE;
			}

			offtout((scan-lenb)-(lastscan+lenf),buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK) {
				zend_error(E_ERROR, "BZ2_bzWrite, bz2err = %d", bz2err);
				RETURN_FALSE;
			}

			offtout((pos-lenb)-(lastpos+lenf),buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK) {
				zend_error(E_ERROR, "BZ2_bzWrite, bz2err = %d", bz2err);
				RETURN_FALSE;
			}

			lastscan=scan-lenb;
			lastpos=pos-lenb;
			lastoffset=pos-scan;
		};
	};
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);

	if (bz2err != BZ_OK) {
		zend_error(E_ERROR, "BZ2_bzWriteClose, bz2err = %d", bz2err);
		RETURN_FALSE;
	}

	/* Compute size of compressed ctrl data */
	if ((len = ftello(pf)) == -1) {
		zend_error(E_ERROR, "Internal error: ftello", 0 TSRMLS_CC);
		RETURN_FALSE;
	}
	offtout(len-32, header + 8);

	/* Write compressed diff data */
	if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL) {
		zend_error(E_ERROR, "BZ2_bzWriteOpen, bz2err = %d", bz2err);
		RETURN_FALSE;
	}
	BZ2_bzWrite(&bz2err, pfbz2, db, dblen);
	if (bz2err != BZ_OK) {
		zend_error(E_ERROR, "BZ2_bzWrite, bz2err = %d", bz2err);
		RETURN_FALSE;
	}
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK) {
		zend_error(E_ERROR, "BZ2_bzWriteClose, bz2err = %d", bz2err);
		RETURN_FALSE;
	}

	/* Compute size of compressed diff data */
	if ((newsize = ftello(pf)) == -1) {
		zend_error(E_ERROR, "Internal error: ftello", 0 TSRMLS_CC);
		RETURN_FALSE;
	}
	offtout(newsize - len, header + 16);

	/* Write compressed extra data */
	if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL) {
		zend_error(E_ERROR, "BZ2_bzWriteOpen, bz2err = %d", bz2err);
		RETURN_FALSE;
	}
	BZ2_bzWrite(&bz2err, pfbz2, eb, eblen);
	if (bz2err != BZ_OK) {
		zend_error(E_ERROR, "BZ2_bzWrite, bz2err = %d", bz2err);
		RETURN_FALSE;
	}
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK) {
		zend_error(E_ERROR, "BZ2_bzWriteClose, bz2err = %d", bz2err);
		RETURN_FALSE;
	}

	/* Seek to the beginning, write the header, and close the file */
	if (fseeko(pf, 0, SEEK_SET)) {
		zend_error(E_ERROR, "Internal error: fseeko", 0 TSRMLS_CC);
		RETURN_FALSE;
	}
	if (fwrite(header, 32, 1, pf) != 1) {
		zend_error(E_ERROR, "Internal error: fwrite(%s)", diff_file);
		RETURN_FALSE;
	}
	if (fclose(pf)) {
		zend_error(E_ERROR, "Internal error: fclose", 0 TSRMLS_CC);
		RETURN_FALSE;
	}

	/* Free the memory we used */
	free(db);
	free(eb);
	free(I);
	free(old);
	free(new);

	RETURN_TRUE;
}

PHP_FUNCTION( bsdiff_patch) {

	FILE * f, * cpf, * dpf, * epf;
	BZFILE * cpfbz2, * dpfbz2, * epfbz2;
	int cbz2err, dbz2err, ebz2err;
	int fd;
	ssize_t oldsize,newsize;
	ssize_t bzctrllen,bzdatalen;
	u_char header[32],buf[8];
	u_char *old, *new;
	off_t oldpos,newpos;
	off_t ctrl[3];
	off_t lenread;
	off_t i;

	u_char *old_file, *new_file, *diff_file;

	zval * self = getThis();
	zval * z_old_file, *z_new_file, *z_diff_file;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", &z_old_file, &z_diff_file, &z_new_file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	old_file = Z_STRVAL_P(z_old_file);
	new_file = Z_STRVAL_P(z_new_file);
	diff_file = Z_STRVAL_P(z_diff_file);

	/* Open patch file */
	if ((f = fopen(diff_file, "r")) == NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error:open file %s faild.", diff_file);
		RETURN_FALSE;
	}

	/*
	File format:
		0	8	"BSDIFF40"
		8	8	X
		16	8	Y
		24	8	sizeof(newfile)
		32	X	bzip2(control block)
		32+X	Y	bzip2(diff block)
		32+X+Y	???	bzip2(extra block)
	with control block a set of triples (x,y,z) meaning "add x bytes
	from oldfile to x bytes from the diff block; copy y bytes from the
	extra block; seek forwards in oldfile by z bytes".
	*/

	/* Read header */
	if (fread(header, 1, 32, f) < 32) {
		if (feof(f)) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
			RETURN_FALSE;
		}
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fread (%s)", diff_file);
		RETURN_FALSE;
	}

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
		RETURN_FALSE;
	}

	/* Read lengths from header */
	bzctrllen=offtin(header+8);
	bzdatalen=offtin(header+16);
	newsize=offtin(header+24);
	if((bzctrllen<0) || (bzdatalen<0) || (newsize<0)) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
		RETURN_FALSE;
	}

	/* Close patch file and re-open it via libbzip2 at the right places */
	if (fclose(f)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fclose(%s)", diff_file);
		RETURN_FALSE;
	}
	if ((cpf = fopen(diff_file, "r")) == NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fopen(%s)", diff_file);
		RETURN_FALSE;
	}
	if (fseeko(cpf, 32, SEEK_SET)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fseeko(%s, %lld)", diff_file, (long long)32);
		RETURN_FALSE;
	}
	if ((cpfbz2 = BZ2_bzReadOpen(&cbz2err, cpf, 0, 0, NULL, 0)) == NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: BZ2_bzReadOpen, bz2err = %d", cbz2err);
		RETURN_FALSE;
	}
	if ((dpf = fopen(diff_file, "r")) == NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fopen(%s)", diff_file);
		RETURN_FALSE;
	}
	if (fseeko(dpf, 32 + bzctrllen, SEEK_SET)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fseeko(%s, %lld)", diff_file, (long long)(32 + bzctrllen));
		RETURN_FALSE;
	}
	if ((dpfbz2 = BZ2_bzReadOpen(&dbz2err, dpf, 0, 0, NULL, 0)) == NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: BZ2_bzReadOpen, bz2err = %d", dbz2err);
		RETURN_FALSE;
	}
	if ((epf = fopen(diff_file, "r")) == NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fopen(%s)", diff_file);
		RETURN_FALSE;
	}
	if (fseeko(epf, 32 + bzctrllen + bzdatalen, SEEK_SET)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fseeko(%s, %lld)", diff_file, (long long)(32 + bzctrllen + bzdatalen));
		RETURN_FALSE;
	}
	if ((epfbz2 = BZ2_bzReadOpen(&ebz2err, epf, 0, 0, NULL, 0)) == NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: BZ2_bzReadOpen, bz2err = %d", ebz2err);
		RETURN_FALSE;
	}

	if(((fd=open(old_file,O_RDONLY,0))<0) ||
			((oldsize=lseek(fd,0,SEEK_END))==-1) ||
			((old=malloc(oldsize+1))==NULL) ||
			(lseek(fd,0,SEEK_SET)!=0) ||
			(read(fd,old,oldsize)!=oldsize) ||
			(close(fd)==-1)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: open (%s)", old_file);
		RETURN_FALSE;
	}

	if((new=malloc(newsize+1))==NULL) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error", 0 TSRMLS_CC);
		RETURN_FALSE;
	}

	oldpos=0;newpos=0;
	while(newpos<newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			lenread = BZ2_bzRead(&cbz2err, cpfbz2, buf, 8);
			if ((lenread < 8) || ((cbz2err != BZ_OK) &&
				(cbz2err != BZ_STREAM_END))) {
				zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
				RETURN_FALSE;
			}
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
		if(newpos+ctrl[0]>newsize) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
			RETURN_FALSE;
		}

		/* Read diff string */
		lenread = BZ2_bzRead(&dbz2err, dpfbz2, new + newpos, ctrl[0]);
		if ((lenread < ctrl[0]) ||
			((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END))) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
			RETURN_FALSE;
		}

		/* Add old data to diff string */
		for(i=0;i<ctrl[0];i++)
			if((oldpos+i>=0) && (oldpos+i<oldsize))
				new[newpos+i]+=old[oldpos+i];

		/* Adjust pointers */
		newpos+=ctrl[0];
		oldpos+=ctrl[0];

		/* Sanity-check */
		if(newpos+ctrl[1]>newsize) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
			RETURN_FALSE;
		}
		/* Read extra string */
		lenread = BZ2_bzRead(&ebz2err, epfbz2, new + newpos, ctrl[1]);
		if ((lenread < ctrl[1]) ||
			((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END))) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Internal error:Corrupt patch.", 0 TSRMLS_CC);
			RETURN_FALSE;
		}

		/* Adjust pointers */
		newpos+=ctrl[1];
		oldpos+=ctrl[2];
	};

	/* Clean up the bzip2 reads */
	BZ2_bzReadClose(&cbz2err, cpfbz2);
	BZ2_bzReadClose(&dbz2err, dpfbz2);
	BZ2_bzReadClose(&ebz2err, epfbz2);
	if (fclose(cpf) || fclose(dpf) || fclose(epf)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: fclose(%s)", diff_file);
		RETURN_FALSE;
	}

	/* Write the new file */
	if(((fd=open(new_file,O_CREAT|O_TRUNC|O_WRONLY,0666))<0) ||
		(write(fd,new,newsize)!=newsize) || (close(fd)==-1)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, "Internal error: write file (%s)", new_file);
		RETURN_FALSE;
	}
	free(new);
	free(old);

	RETURN_TRUE;
}

/* {{{ bsdiff_functions[]
 *
 * Every user visible function must have an entry in bsdiff_functions[].
 */
const zend_function_entry bsdiff_functions[] = {
	PHP_FE(bsdiff_diff,	NULL)		/* For testing, remove later. */
	PHP_FE(bsdiff_patch, NULL)
	PHP_FE_END /* Must be the last line in bsdiff_functions[] */
};
/* }}} */

/* {{{ bsdiff_module_entry
 */
zend_module_entry bsdiff_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
		STANDARD_MODULE_HEADER,
#endif
		"bsdiff",
		bsdiff_functions,
		PHP_MINIT(bsdiff),
		PHP_MSHUTDOWN(bsdiff),
		PHP_RINIT(bsdiff),		/* Replace with NULL if there's nothing to do at request start */
		PHP_RSHUTDOWN(bsdiff),	/* Replace with NULL if there's nothing to do at request end */
		PHP_MINFO(bsdiff),
#if ZEND_MODULE_API_NO >= 20010901
		"0.1", /* Replace with version number for your extension */
#endif
		STANDARD_MODULE_PROPERTIES };
/* }}} */

#ifdef COMPILE_DL_BSDIFF
ZEND_GET_MODULE(bsdiff)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
 PHP_INI_BEGIN()
 STD_PHP_INI_ENTRY("bsdiff.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_bsdiff_globals, bsdiff_globals)
 STD_PHP_INI_ENTRY("bsdiff.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_bsdiff_globals, bsdiff_globals)
 PHP_INI_END()
 */
/* }}} */

/* {{{ php_bsdiff_init_globals
 */
/* Uncomment this function if you have INI entries
 static void php_bsdiff_init_globals(zend_bsdiff_globals *bsdiff_globals)
 {
 bsdiff_globals->global_value = 0;
 bsdiff_globals->global_string = NULL;
 }
 */
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION( bsdiff) {
	/* If you have INI entries, uncomment these lines 
	 REGISTER_INI_ENTRIES();
	 */
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION( bsdiff) {
	/* uncomment this line if you have INI entries
	 UNREGISTER_INI_ENTRIES();
	 */
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION( bsdiff) {
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION( bsdiff) {
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION( bsdiff) {
	php_info_print_table_start();
	php_info_print_table_header(2, "bsdiff support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	 DISPLAY_INI_ENTRIES();
	 */
}
/* }}} */

/* Remove the following function when you have succesfully modified config.m4
 so that your module can be compiled into PHP, it exists only for testing
 purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_bsdiff_compiled(string arg)
 Return a string to confirm that the module is compiled in */
/*PHP_FUNCTION(confirm_bsdiff_compiled)
 {
 char *arg = NULL;
 int arg_len, len;
 char *strg;

 if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
 return;
 }

 len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "bsdiff", arg);
 RETURN_STRINGL(strg, len, 0);
 }*/
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
 unfold functions in source code. See the corresponding marks just before
 function definition, where the functions purpose is also documented. Please
 follow this convention for the convenience of others editing your code.
 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
