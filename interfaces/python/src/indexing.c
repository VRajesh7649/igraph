/* vim:set ts=4 sw=2 sts=2 et:  */
/* 
   IGraph library - Python interface.
   Copyright (C) 2006-2011  Tamas Nepusz <ntamas@gmail.com>
   5 Avenue Road, Staines, Middlesex, TW18 3AW, United Kingdom
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc.,  51 Franklin Street, Fifth Floor, Boston, MA 
   02110-1301 USA

*/

#include "attributes.h"
#include "convert.h"
#include "error.h"
#include "indexing.h"
#include "pyhelpers.h"

static PyObject* igraphmodule_i_Graph_adjmatrix_indexing_get_value_for_vertex_pair(
    igraph_t* graph, igraph_integer_t from, igraph_integer_t to, PyObject* values) {
  igraph_integer_t eid;
  PyObject* result;

  /* Retrieving a single edge */
  igraph_get_eid(graph, &eid, from, to, /* directed = */1, /* error = */0);
  if (eid >= 0) {
    /* Edge found, get the value of the attribute */
    if (values == 0) {
      return PyInt_FromLong(1L);
    } else {
      result = PyList_GetItem(values, eid);
      Py_XINCREF(result);
      return result;
    }
  } else {
    /* No such edge, return zero */
    return PyInt_FromLong(0L);
  }
}

static PyObject* igraphmodule_i_Graph_adjmatrix_indexing_row(igraph_t* graph, 
    igraph_integer_t from, igraph_vs_t* to, igraph_neimode_t neimode,
    PyObject* values);

PyObject* igraphmodule_Graph_adjmatrix_indexing(igraph_t* graph,
        PyObject* row_index, PyObject* column_index, PyObject* attr_name) {
    PyObject *result = 0, *values;
    igraph_vs_t vs1, vs2;
    igraph_integer_t vid1 = -1, vid2 = -1;
    char* attr;

    if (attr_name == 0) {
      /* Using the "weight" attribute by default */
      attr = strdup("weight");
    } else {
      /* Specifying the name of the attribute */
      attr = PyObject_ConvertToCString(attr_name);
    }

    if (igraphmodule_PyObject_to_vs_t(row_index, &vs1, graph, 0, &vid1))
      return NULL;
    if (igraphmodule_PyObject_to_vs_t(column_index, &vs2, graph, 0, &vid2))
      return NULL;

    values = igraphmodule_get_edge_attribute_values(graph, attr);

    if (vid1 >= 0 && vid2 >= 0) {
      /* Retrieving an edge between vid1 and vid2 */
      result = igraphmodule_i_Graph_adjmatrix_indexing_get_value_for_vertex_pair(
          graph, vid1, vid2, values);
    } else if (vid1 >= 0) {
      /* Retrieving the successors of vid1 */
      result = igraphmodule_i_Graph_adjmatrix_indexing_row(
          graph, vid1, &vs2, IGRAPH_OUT, values);
    } else if (vid2 >= 0) {
      /* Retrieving the predecessors of vid2 */
      result = igraphmodule_i_Graph_adjmatrix_indexing_row(
          graph, vid2, &vs1, IGRAPH_IN, values);
    } else {
      /* Retrieving a submatrix */
      result = Py_None; Py_INCREF(result);
    }

    igraph_vs_destroy(&vs1);
    igraph_vs_destroy(&vs2);

    return result;
}

static PyObject* igraphmodule_i_Graph_adjmatrix_indexing_row(igraph_t* graph, 
    igraph_integer_t from, igraph_vs_t* to, igraph_neimode_t neimode,
    PyObject* values) {
  igraph_vector_t eids;
  igraph_vit_t vit;
  PyObject *result = 0, *item;
  long int i, n, eid, v;

  if (igraph_vs_is_all(to)) {
    /* Simple case: all edges */
    IGRAPH_PYCHECK(igraph_vector_init(&eids, 0));
    IGRAPH_FINALLY(igraph_vector_destroy, &eids);
    IGRAPH_PYCHECK(igraph_incident(graph, &eids, from, neimode));
    
    n = igraph_vector_size(&eids);
    result = PyList_Zeroes(igraph_vcount(graph));
    if (result == 0) {
      IGRAPH_FINALLY_FREE();
      return 0;
    }

    for (i = 0; i < n; i++) {
      eid = VECTOR(eids)[i];
      v = IGRAPH_OTHER(graph, eid, from);
      if (values)
        item = PyList_GetItem(values, eid);
      else
        item = PyInt_FromLong(1);
      Py_INCREF(item);
      PyList_SetItem(result, v, item);   /* reference stolen here */
    }

    IGRAPH_FINALLY_CLEAN(1);
    igraph_vector_destroy(&eids);

    return result;
  }

  /* More complicated case: only some vertices */
  IGRAPH_PYCHECK(igraph_vit_create(graph, *to, &vit));
  IGRAPH_FINALLY(igraph_vit_destroy, &vit);

  result = PyList_New(0);
  if (result == 0) {
    IGRAPH_FINALLY_FREE();
    return 0;
  }

  while (!IGRAPH_VIT_END(vit)) {
    v = IGRAPH_VIT_GET(vit);
    item = igraphmodule_i_Graph_adjmatrix_indexing_get_value_for_vertex_pair(
        graph, from, v, values);
    if (item == 0) {
      IGRAPH_FINALLY_FREE();
      Py_DECREF(result);
      return 0;
    }
    PyList_Append(result, item);
    Py_DECREF(item);
    IGRAPH_VIT_NEXT(vit);
  }

  igraph_vit_destroy(&vit);
  IGRAPH_FINALLY_CLEAN(1);

  return result;
}

