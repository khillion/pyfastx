#include "sequence.h"
#include "structmember.h"
#include "util.h"

PyObject *pyfastx_sequence_new(PyTypeObject *type, PyObject *args, PyObject *kwargs){
	pyfastx_Sequence *obj = (pyfastx_Sequence *)type->tp_alloc(type, 0);
	return (PyObject *)obj;
}

PyObject *pyfastx_sequence_iter(pyfastx_Sequence* self){
	gzseek(self->index->gzfd, self->offset, SEEK_SET);
	self->ks = ks_init(self->index->gzfd);
	Py_INCREF(self);
	return (PyObject *)self;
}

PyObject *pyfastx_sequence_next(pyfastx_Sequence* self){
	kstring_t seq = {0, 0, 0};
	if(self->start != 1 || self->end != self->seq_len){
		int c;
		while((c = ks_getc(self->ks)) >= 0 && c != '>'){
			if(c == '\n'){
				continue;
			} else {
				return Py_BuildValue("C", c);
			}
		}
		return NULL;
	} else {
		if(ks_getuntil(self->ks, '\n', &seq, 0) >= 0){
			if(seq.s[0] != '>'){
				if(self->index->uppercase){
					upper_string(seq.s);
				}
				return Py_BuildValue("s", seq.s);
			}
		}
	}
	return NULL;
}

int pyfastx_sequence_length(pyfastx_Sequence* self){
	return self->seq_len;
}

char *pyfastx_sequence_acquire(pyfastx_Sequence* self){
	char *seq = pyfastx_index_get_sub_seq(self->index, self->name, self->offset, self->byte_len, self->start, self->end, self->normal);
	char *seq1 = malloc(strlen(seq)+1);
	strcpy(seq1, seq);
	return seq1;
}

PyObject *pyfastx_sequence_get_name(pyfastx_Sequence* self, void* closure){
	if(self->start == 1 && self->end == self->seq_len){
		return Py_BuildValue("s", self->name);
	} else {
		return PyUnicode_FromFormat("%s:%d-%d", self->name, self->start, self->end);
	}
}

PyObject *pyfastx_sequence_description(pyfastx_Sequence* self, void* closure){
	sqlite3_stmt *stmt;
	const char *sql = "SELECT descript FROM seq WHERE seqid=? LIMIT 1";
	sqlite3_prepare_v2(self->index->index_db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, self->name, -1, NULL);
	
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		char *header = (char *)sqlite3_column_text(stmt, 0);
		sqlite3_finalize(stmt);
		return Py_BuildValue("s", header);
	}

	Py_RETURN_NONE;
}

PyObject *pyfastx_sequence_seq(pyfastx_Sequence* self, void* closure){
	char *seq = pyfastx_index_get_sub_seq(self->index, self->name, self->offset, self->byte_len, self->start, self->end, self->normal);
	return Py_BuildValue("s", seq);
}

PyObject *pyfastx_sequence_reverse(pyfastx_Sequence* self, void* closure){
	char *seq = pyfastx_sequence_acquire(self);
	reverse_seq(seq);
	return Py_BuildValue("s", seq);
}

PyObject *pyfastx_sequence_complement(pyfastx_Sequence* self, void* closure){
	char *seq = pyfastx_sequence_acquire(self);
	complement_seq(seq);
	return Py_BuildValue("s", seq);
}

//complement reverse sequence
PyObject *pyfastx_sequence_antisense(pyfastx_Sequence* self, void* closure){
	char *seq = pyfastx_sequence_acquire(self);
	reverse_seq(seq);
	complement_seq(seq);
	return Py_BuildValue("s", seq);
}

PyObject *pyfastx_sequence_repr(pyfastx_Sequence* self){
	if(self->start == 1 && self->end == self->seq_len){
		return PyUnicode_FromFormat("<Sequence> %s with length of %d", self->name, self->seq_len);
	} else {
		return PyUnicode_FromFormat("<Sequence> %s from %d to %d", self->name, self->start, self->end);
	}
}

PyObject *pyfastx_sequence_str(pyfastx_Sequence* self){
	return pyfastx_sequence_seq(self, NULL);
}

PyObject *pyfastx_seqeunce_subscript(pyfastx_Sequence* self, PyObject* item){
	char *seq;
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		
		if (i == -1 && PyErr_Occurred()){
			return NULL;
		}

		if (i < 0) {
			i += self->seq_len;
		}

		seq = pyfastx_index_get_sub_seq(self->index, self->name, self->offset, self->byte_len, self->start, self->end, self->normal);
		return Py_BuildValue("C", *(seq+i));
	
	} else if (PySlice_Check(item)) {
		Py_ssize_t slice_start, slice_stop, slice_step, slice_len;

		if(PySlice_Unpack(item, &slice_start, &slice_stop, &slice_step) < 0) {
			return NULL;
		}

		slice_len = PySlice_AdjustIndices(self->seq_len, &slice_start, &slice_stop, slice_step);

		if (slice_len <= 0) {
			Py_RETURN_NONE;
		}

		if(slice_step != 1) {
			Py_RETURN_NONE;
		}

		//create a new sequence
		pyfastx_Sequence *seq = PyObject_New(pyfastx_Sequence, &pyfastx_SequenceType);
		if(!seq){
			return NULL;
		}
		
		//seq->name = self->name;
		seq->start = slice_start + self->start;
		seq->end = slice_stop + self->start - 1;
		//seq->name = (char *)malloc(strlen(self->name) + 25);
		seq->name = self->name;
		//sprintf(seq->name, "%s:%d-%d", self->name, seq->start, seq->end);
		seq->seq_len = slice_stop - slice_start;
		seq->line_len = self->line_len;
		seq->end_len = self->end_len;
		seq->normal = self->normal;
		seq->offset = self->offset;
		seq->byte_len = self->byte_len;
		seq->index = self->index;

		if (self->normal) {
			//line_num = seq->seq_len / (seq->line_len - seq->end_len);
			//tail_num = seq->seq_len % (seq->line_len - seq->end_len);
			//seq->offset = seq->byte_len + seq->start + (seq->start / (seq->line_len - seq->end_len)) * seq->end_len - 1;
			//seq->byte_len = line_num * seq->line_len + tail_num;
			int line_num = (slice_start + 1)/(self->line_len - self->end_len);
			seq->offset = self->offset + slice_start + self->end_len*line_num;
			seq->byte_len = seq->seq_len + seq->seq_len/self->line_len*self->end_len;
		}

		Py_INCREF(seq);
		return (PyObject *)seq;
	} else {
		return NULL;
	}

}

static PyMappingMethods pyfastx_sequence_as_mapping = {
	(lenfunc)pyfastx_sequence_length,
	(binaryfunc)pyfastx_seqeunce_subscript,
	0
};

/*static PyMethodDef pyfastx_sequence_methods[] = {
	{"sub_seq", (PyCFunction)pyfastx_sequence_sub_seq, METH_VARARGS},
	{NULL, NULL, 0, NULL}
};*/

static PyGetSetDef pyfastx_sequence_getsets[] = {
	{"name", (getter)pyfastx_sequence_get_name, NULL, NULL, NULL},
	{"seq", (getter)pyfastx_sequence_seq, NULL, NULL, NULL},
	{"reverse", (getter)pyfastx_sequence_reverse, NULL, NULL, NULL},
	{"complement", (getter)pyfastx_sequence_complement, NULL, NULL, NULL},
	{"antisense", (getter)pyfastx_sequence_antisense, NULL, NULL, NULL},
	{"description", (getter)pyfastx_sequence_description, NULL, NULL, NULL},
	{NULL}
};

static PyMemberDef pyfastx_sequence_members[] = {
	//{"name", T_STRING, offsetof(pyfastx_Sequence, name), READONLY},
	{"start", T_INT, offsetof(pyfastx_Sequence, start), READONLY},
	{"end", T_INT, offsetof(pyfastx_Sequence, end), READONLY},
	//{"length", T_INT, offsetof(pyfastx_Sequence, seq_len), READONLY},
	{"gc_content", T_FLOAT, offsetof(pyfastx_Sequence, gc_content), READONLY},
	{"composition", T_OBJECT, offsetof(pyfastx_Sequence, composition), READONLY},
	{NULL}
};

PyTypeObject pyfastx_SequenceType = {
    //PyVarObject_HEAD_INIT(&PyType_Type, 0)
    PyVarObject_HEAD_INIT(NULL, 0)
    "Sequence",                     /* tp_name */
    sizeof(pyfastx_Sequence),       /* tp_basicsize */
    0,                              /* tp_itemsize */
    0,                              /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_reserved */
    (reprfunc)pyfastx_sequence_repr,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    &pyfastx_sequence_as_mapping,   /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    (reprfunc)pyfastx_sequence_str,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    0,                              /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    (getiterfunc)pyfastx_sequence_iter,                              /* tp_iter */
    (iternextfunc)pyfastx_sequence_next,                              /* tp_iternext */
    0,       /* tp_methods */
    pyfastx_sequence_members,       /* tp_members */
    pyfastx_sequence_getsets,       /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    PyType_GenericAlloc,            /* tp_alloc */
    pyfastx_sequence_new,           /* tp_new */
};
