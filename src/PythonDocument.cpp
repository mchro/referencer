
#include <iostream>

#include <Python.h>
#include <structmember.h>

#include "Document.h"
#include "Utility.h"

#include "PythonDocument.h"

static PyObject *referencer_document_get_key (PyObject *self, PyObject *args)
{
	Glib::ustring value = ((referencer_document*)self)->doc_->getKey ();
	return PyUnicode_FromString(value.c_str());
}


static PyObject *referencer_document_set_key (PyObject *self, PyObject *args)
{
	PyObject *value = PyTuple_GetItem (args, 0);
	((referencer_document*)self)->doc_->setKey (PyUnicode_AsUTF8(value));
	return Py_BuildValue ("i", 0);
}


static PyObject *referencer_document_get_filename (PyObject *self, PyObject *args)
{
	Glib::ustring value = ((referencer_document*)self)->doc_->getFileName ();
	return PyUnicode_FromString(value.c_str());
}


static PyObject *referencer_document_set_filename (PyObject *self, PyObject *args)
{
	PyObject *value = PyTuple_GetItem (args, 0);
	((referencer_document*)self)->doc_->setFileName (PyUnicode_AsUTF8(value));
	return Py_BuildValue ("i", 0);
}

static PyObject *referencer_document_get_notes(PyObject *self, PyObject *args)
{
	Glib::ustring value = ((referencer_document*)self)->doc_->getNotes ();
	return PyUnicode_FromString(value.c_str());
}


static PyObject *referencer_document_set_notes (PyObject *self, PyObject *args)
{
	PyObject *value = PyTuple_GetItem (args, 0);
	((referencer_document*)self)->doc_->setNotes (PyUnicode_AsUTF8(value));
	return Py_BuildValue ("i", 0);
}


static PyObject *referencer_document_get_type (PyObject *self, PyObject *args)
{
	try {
		Glib::ustring value = ((referencer_document*)self)->doc_->getBibData().getType();
		return PyUnicode_FromString(value.c_str());
	} catch (std::exception &ex) {
		PyErr_SetString (PyExc_KeyError, ex.what());
        Py_INCREF(Py_None);
        return Py_None;
	}
}

static PyObject *referencer_document_set_type (PyObject *self, PyObject *args)
{
	PyObject *value = PyTuple_GetItem (args, 0);
	((referencer_document*)self)->doc_->getBibData().setType (PyUnicode_AsUTF8(value));
	return Py_BuildValue ("i", 0);
}


static PyObject *referencer_document_get_field (PyObject *self, PyObject *args)
{
	PyObject *fieldName = PyTuple_GetItem (args, 0);

	try {
		Glib::ustring value = ((referencer_document*)self)->doc_->getField (PyUnicode_AsUTF8(fieldName));
		return PyUnicode_FromString(value.c_str());
	}
	catch (std::range_error &ex) { /* unknown field */
		Py_INCREF(Py_None);
		return Py_None;
	}
	catch (std::exception &ex) {
		PyErr_SetString (PyExc_KeyError, ex.what());
		return NULL;
	}
}

static PyObject *referencer_document_get_fields (PyObject *self, PyObject *args)
{
	/* TODO */
	return NULL;
}

static PyObject *referencer_document_set_fields (PyObject *self, PyObject *args)
{
	/* TODO */
	return NULL;
}

static PyObject *referencer_document_set_field (PyObject *self, PyObject *args)
{
	PyObject *fieldName = PyTuple_GetItem (args, 0);
	PyObject *value = PyTuple_GetItem (args, 1);
	((referencer_document*)self)->doc_->setField (PyUnicode_AsUTF8(fieldName), PyUnicode_AsUTF8(value));
	return Py_BuildValue ("i", 0);
}


static PyObject *referencer_document_parse_bibtex (PyObject *self, PyObject *args)
{
	PyObject *bibtex = PyTuple_GetItem (args, 0);
	((referencer_document*)self)->doc_->parseBibtex (PyUnicode_AsUTF8(bibtex));
	return Py_BuildValue ("i", 0);
}

static PyObject *referencer_document_print_bibtex (PyObject *self, PyObject *args)
{
	/* Parse arguments */
	Document *doc = ((referencer_document*)self)->doc_;
	bool useBraces = (PyTuple_GetItem (args, 0) == Py_True);
	bool utf8 = (PyTuple_GetItem (args, 1) == Py_True);

	/* Get bibtex string */
	Glib::ustring bibtex = doc->printBibtex (useBraces, utf8);

	/* Convert for output */
	/* FIXME: will this work with utf8 true? */
	return PyUnicode_FromString (bibtex.c_str());
}

static void referencer_document_dealloc (PyObject *self)
{
	DEBUG (">> referencer_document_dealloc");
}

static PyObject *referencer_document_string (PyObject *self)
{
	return PyUnicode_FromString ("Referencer object representing a single document");
}

static int referencer_document_init (PyObject *self, PyObject *args, PyObject *kwds)
{
	DEBUG (">> referencer_document_init");

	return 0;
}


static PyMemberDef referencer_document_members[] = {
	{"ptr", T_INT, offsetof(referencer_document, doc_), 0, "Pointer to C++ Document"},
	{NULL}
};

static PyMethodDef referencer_document_methods[] = {
	{"get_type",	referencer_document_get_type,	METH_VARARGS, "Get the document type"},
	{"set_type",	referencer_document_set_type,	METH_VARARGS, "Set the document type"},
	{"get_field", referencer_document_get_field, METH_VARARGS, "Get a field"},
	{"get_fields", referencer_document_get_fields, METH_VARARGS, "Get a dictionary of all fields"},
	{"set_field", referencer_document_set_field, METH_VARARGS, "Set a field"},
	{"set_fields", referencer_document_set_fields, METH_VARARGS, "Set all fields from a dictionary"},
	{"get_key", referencer_document_get_key, METH_VARARGS, "Get the key"},
	{"set_key", referencer_document_set_key, METH_VARARGS, "Set the key"},
	{"get_filename", referencer_document_get_filename, METH_VARARGS, "Get the filename"},
	{"set_filename", referencer_document_set_filename, METH_VARARGS, "Set the filename"},
	{"get_notes", referencer_document_get_notes, METH_VARARGS, "Get the notes"},
	{"set_notes", referencer_document_set_notes, METH_VARARGS, "Set the notes"},
	{"parse_bibtex", referencer_document_parse_bibtex, METH_VARARGS, "Set fields from bibtex string"},
	{"print_bibtex", referencer_document_print_bibtex, METH_VARARGS, "Print bibtex representation of document"},
	{NULL, NULL, 0, NULL}
};

PyTypeObject t_referencer_document = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"referencer.document",
	sizeof (referencer_document),
	0,
	(destructor)referencer_document_dealloc,
	0,
	0,
	0,
	0,
	(reprfunc)referencer_document_string,
	0,
	0,
	0,
	0,
	0,
	0,
	PyObject_GenericGetAttr,
	PyObject_GenericSetAttr,
	0,
	Py_TPFLAGS_DEFAULT,
	"Referencer Document",
	0,
	0,
	0,
	0,
	0,
	0,
	referencer_document_methods,
	referencer_document_members,
	0,
	0,
	0,
	0,
	0,
	0,
	(initproc)referencer_document_init,
	PyType_GenericAlloc,
	PyType_GenericNew,
	PyObject_Del
};
