#ifndef PYFASTX_FASTA_H
#define PYFASTX_FASTA_H
#include "Python.h"
#include "zlib.h"
#include "kseq.h"
#include "index.h"

//make sequence iterator
typedef struct {
	PyObject_HEAD

	//fasta or fastq file path and name
	char* file_name;

	//always output uppercase sequence
	int uppercase;

	//total sequence counts
	int seq_counts;

	//total sequence length (bp)
	long seq_length;

	//a float for GC content (%)
	float gc_content;

	//a dict for storing A T G C N (unknown base) counts in fasta
	PyObject* composition;

	//index for fast random access to sequence
	pyfastx_Index* index;

} pyfastx_Fasta;

extern PyTypeObject pyfastx_FastaType;

void pyfastx_calc_fasta_attrs(pyfastx_Fasta *self);
void pyfastx_fasta_dealloc(pyfastx_Fasta *self);
int pyfastx_fasta_length(pyfastx_Fasta *self);


PyObject *pyfastx_fasta_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);
PyObject *pyfastx_fasta_iter(pyfastx_Fasta *self);
PyObject *pyfastx_fasta_next(pyfastx_Fasta *self);
PyObject *pyfastx_fasta_build_index(pyfastx_Fasta *self, PyObject *args, PyObject *kwargs);
PyObject *pyfastx_fasta_rebuild_index(pyfastx_Fasta *self, PyObject *args, PyObject *kwargs);
PyObject *pyfastx_fasta_subscript(pyfastx_Fasta *self, PyObject *item);
PyObject *pyfastx_fasta_fetch(pyfastx_Fasta *self, PyObject *args, PyObject *kwargs);

#endif