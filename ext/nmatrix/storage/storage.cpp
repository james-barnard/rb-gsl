/////////////////////////////////////////////////////////////////////
// = NMatrix
//
// A linear algebra library for scientific computation in Ruby.
// NMatrix is part of SciRuby.
//
// NMatrix was originally inspired by and derived from NArray, by
// Masahiro Tanaka: http://narray.rubyforge.org
//
// == Copyright Information
//
// SciRuby is Copyright (c) 2010 - 2012, Ruby Science Foundation
// NMatrix is Copyright (c) 2012, Ruby Science Foundation
//
// Please see LICENSE.txt for additional copyright notices.
//
// == Contributing
//
// By contributing source code to SciRuby, you agree to be bound by
// our Contributor Agreement:
//
// * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
//
// == storage.cpp
//
// Code that is used by or involves more then one storage type.

/*
 * Standard Includes
 */

/*
 * Project Includes
 */

#include "data/data.h"

#include "storage.h"

/*
 * Macros
 */

/*
 * Global Variables
 */

const char* const STYPE_NAMES[NUM_STYPES] = {
	"dense",
	"list",
	"yale"
};



/*
 * Forward Declarations
 */

template <typename LDType, typename RDType>
static void dense_storage_cast_copy_list_contents_template(LDType* lhs, const LIST* rhs, RDType* default_val,
	size_t& pos, const size_t* shape, size_t rank, size_t max_elements, size_t recursions);

template <typename LDType, typename RDType>
static void dense_storage_cast_copy_list_default_template(LDType* lhs, RDType* default_val, size_t& pos,
  const size_t* shape, size_t rank, size_t max_elements, size_t recursions);

/*
 * Functions
 */

/////////////////////////
// Templated Functions //
/////////////////////////
/*
 * Convert (by creating a copy) from list storage to dense storage.
 */
template <typename LDType, typename RDType>
DENSE_STORAGE* dense_storage_from_list_template(const LIST_STORAGE* rhs, dtype_t l_dtype) {

  // allocate and copy shape
  size_t* shape = ALLOC_N(size_t, rhs->rank);
  memcpy(shape, rhs->shape, rhs->rank * sizeof(size_t));

  DENSE_STORAGE* lhs = dense_storage_create(l_dtype, shape, rhs->rank, NULL, 0);

  // Position in lhs->elements.
  size_t pos = 0;
  size_t max_elements = storage_count_max_elements(rhs->rank, rhs->shape);

//static void dense_storage_cast_copy_list_contents_template(LDType* lhs, const LIST* rhs, RDType* default_val, size_t& pos, const size_t* shape, size_t rank, size_t max_elements, size_t recursions)
  // recursively copy the contents
  dense_storage_cast_copy_list_contents_template<LDType,RDType>(reinterpret_cast<LDType*>(lhs->elements),
                                                                rhs->rows,
                                                                reinterpret_cast<RDType*>(rhs->default_val),
                                                                pos, shape, lhs->rank, max_elements, rhs->rank-1);

  return lhs;
}

STORAGE* dense_storage_from_list(const STORAGE* right, dtype_t l_dtype) {
	NAMED_LR_DTYPE_TEMPLATE_TABLE(ttable, dense_storage_from_list_template, DENSE_STORAGE*, const LIST_STORAGE* rhs, dtype_t l_dtype);

	return (STORAGE*)ttable[l_dtype][right->dtype]((const LIST_STORAGE*)right, l_dtype);
}


/*
 * Create/allocate dense storage, copying into it the contents of a Yale matrix.
 */
template <typename LDType, typename RDType, typename RIType>
DENSE_STORAGE* dense_storage_from_yale_template(const YALE_STORAGE* rhs, dtype_t l_dtype) {

  // Position in rhs->elements.
  RIType* rhs_ija = reinterpret_cast<RIType*>(rhs->ija);
  RDType* rhs_a   = reinterpret_cast<RDType*>(rhs->a);
  
  // Allocate and set shape.
  size_t* shape = ALLOC_N(size_t, rhs->rank);
  memcpy(shape, rhs->shape, rhs->rank * sizeof(size_t));

  DENSE_STORAGE* lhs = dense_storage_create(l_dtype, shape, rhs->rank, NULL, 0);
  LDType* lhs_elements = reinterpret_cast<LDType*>(lhs->elements);

  // Position in dense to write to.
  size_t pos = 0;

  LDType LCAST_ZERO = rhs_a[rhs->shape[0]];

  // Walk through rows. For each entry we set in dense, increment pos.
  for (RIType i = 0; i < rhs->shape[0]; ++i) {

    // Position in yale array
    RIType ija = rhs_ija[i];

    if (ija == rhs_ija[i+1]) { // Check boundaries of row: is row empty?

			// Write zeros in each column.
			for (RIType j = 0; j < rhs->shape[1]; ++j) { // Move to next dense position.

        // Fill in zeros (except for diagonal)
        if (i == j) lhs_elements[pos] = rhs_a[i];
				else        lhs_elements[pos] = LCAST_ZERO;

				++pos;
      }

    } else {
      // Row contains entries: write those in each column, interspersed with zeros.
      RIType jj = rhs_ija[ija];

			for (size_t j = 0; j < rhs->shape[1]; ++j) {
        if (i == j) {
          lhs_elements[pos] = rhs_a[i];

        } else if (j == jj) {
          lhs_elements[pos] = rhs_a[ija]; // Copy from rhs.

          // Get next.
          ++ija;

          // Increment to next column ID (or go off the end).
          if (ija < rhs_ija[i+1]) jj = rhs_ija[ija];
          else               	    jj = rhs->shape[1];
          
        } else { // j < jj

          // Insert zero.
          lhs_elements[pos] = LCAST_ZERO;
        }
        
        // Move to next dense position.
        ++pos;
      }
    }
  }

  return lhs;
}

STORAGE* dense_storage_from_yale(const STORAGE* right, dtype_t l_dtype) {
	NAMED_LRI_DTYPE_TEMPLATE_TABLE(ttable, dense_storage_from_yale_template, DENSE_STORAGE*, const YALE_STORAGE* rhs, dtype_t l_dtype);

  const YALE_STORAGE* casted_right = reinterpret_cast<const YALE_STORAGE*>(right);
	return reinterpret_cast<STORAGE*>(ttable[l_dtype][right->dtype][casted_right->itype](casted_right, l_dtype));
}


/*
 * Creation of list storage from dense storage.
 */
template <typename LDType, typename RDType>
LIST_STORAGE* list_storage_from_dense_template(const DENSE_STORAGE* rhs, dtype_t l_dtype) {

  LDType* l_default_val = ALLOC_N(LDType, 1);
  RDType* r_default_val = ALLOCA_N(RDType, 1); // clean up when finished with this function

  // allocate and copy shape and coords
  size_t *shape  = ALLOC_N(size_t, rhs->rank),
         *coords = ALLOC_N(size_t, rhs->rank);

  memcpy(shape, rhs->shape, rhs->rank * sizeof(size_t));
  memset(coords, 0, rhs->rank * sizeof(size_t));

  // set list default_val to 0
  if (l_dtype == RUBYOBJ)  	*l_default_val = INT2FIX(0);
  else    	                *l_default_val = 0;

  // need test default value for comparing to elements in dense matrix
  if (rhs->dtype == l_dtype)  	  *r_default_val = *l_default_val;
  else if (rhs->dtype == RUBYOBJ) *r_default_val = INT2FIX(0);
  else  	                        *r_default_val = 0;

  LIST_STORAGE* lhs = list_storage_create(l_dtype, shape, rhs->rank, l_default_val);

  //lhs->rows = list_create();
  size_t pos = 0;
  list_storage_cast_copy_contents_dense_template<LDType,RDType>(lhs->rows,
                                                                reinterpret_cast<const RDType*>(rhs->elements),
                                                                r_default_val,
                                                                pos, coords, rhs->shape, rhs->rank, rhs->rank - 1);

  return lhs;
}

STORAGE* list_storage_from_dense(const STORAGE* right, dtype_t l_dtype) {
	NAMED_LR_DTYPE_TEMPLATE_TABLE(ttable, list_storage_from_dense_template, LIST_STORAGE*, const DENSE_STORAGE*, dtype_t);

	return (STORAGE*)ttable[l_dtype][right->dtype]((DENSE_STORAGE*)right, l_dtype);
}


/*
 * Creation of list storage from yale storage.
 */
template <typename LDType, typename RDType, typename RIType>
LIST_STORAGE* list_storage_from_yale_template(const YALE_STORAGE* rhs, dtype_t l_dtype) {
  // allocate and copy shape
  size_t *shape = ALLOC_N(size_t, rhs->rank);
  shape[0] = rhs->shape[0]; shape[1] = rhs->shape[1];

  RDType* rhs_a    = reinterpret_cast<RDType*>(rhs->a);
  RDType R_ZERO    = rhs_a[ rhs->shape[0] ];

  // copy default value from the zero location in the Yale matrix
  LDType* default_val = ALLOC_N(LDType, 1);
  *default_val        = R_ZERO;

  LIST_STORAGE* lhs = list_storage_create(l_dtype, shape, rhs->rank, default_val);

  if (rhs->rank != 2)    rb_raise(nm_eStorageTypeError, "Can only convert matrices of rank 2 from yale.");

  RIType* rhs_ija  = reinterpret_cast<RIType*>(rhs->ija);

  NODE *last_added, *last_row_added = NULL;

  // Walk through rows and columns as if RHS were a dense matrix
  for (RIType i = 0; i < rhs->shape[0]; ++i) {

    // Get boundaries of beginning and end of row
    RIType ija      = rhs_ija[i],
           ija_next = rhs_ija[i+1];

    // Are we going to need to add a diagonal for this row?
    bool add_diag = false;
    if (rhs_a[i] != R_ZERO) add_diag = true;
		
    if (ija < ija_next || add_diag) {

      LIST* curr_row = list_create();

      LDType* insert_val;

      while (ija < ija_next) {
        RIType jj = rhs_ija[ija]; // what column number is this?

        // Is there a nonzero diagonal item between the previously added item and the current one?
        if (jj > i && add_diag) {
          // Allocate and copy insertion value
          insert_val = ALLOC_N(LDType, 1);
          *insert_val        = rhs_a[i];

          // insert the item in the list at the appropriate location
          if (last_added) 	last_added = list_insert_after(last_added, i, insert_val);
          else            	last_added = list_insert(curr_row, false, i, insert_val);
					
					// don't add again!
          add_diag = false;
        }

        // now allocate and add the current item
        insert_val  = ALLOC_N(LDType, 1);
        *insert_val = rhs_a[ija];

        if (last_added)    	last_added = list_insert_after(last_added, jj, insert_val);
        else              	last_added = list_insert(curr_row, false, jj, insert_val);

        ++ija; // move to next entry in Yale matrix
      }

      if (add_diag) {
      	// still haven't added the diagonal.
        insert_val = ALLOC_N(LDType, 1);
        *insert_val        = rhs_a[i];

        // insert the item in the list at the appropriate location
        if (last_added)    	last_added = list_insert_after(last_added, i, insert_val);
        else              	last_added = list_insert(curr_row, false, i, insert_val);
      }

      // Now add the list at the appropriate location
      if (last_row_added)  	last_row_added = list_insert_after(last_row_added, i, curr_row);
      else                 	last_row_added = list_insert(lhs->rows, false, i, curr_row);
    }
	
		// end of walk through rows
  }

  return lhs;
}

STORAGE* list_storage_from_yale(const STORAGE* right, dtype_t l_dtype) {
	NAMED_LRI_DTYPE_TEMPLATE_TABLE(ttable, list_storage_from_yale_template, LIST_STORAGE*, const YALE_STORAGE* rhs, dtype_t l_dtype);

  const YALE_STORAGE* casted_right = reinterpret_cast<const YALE_STORAGE*>(right);

	return (STORAGE*)ttable[l_dtype][right->dtype][casted_right->itype](casted_right, l_dtype);
}


/*
 * Creation of yale storage from dense storage.
 */
template <typename LDType, typename RDType, typename LIType>
YALE_STORAGE* yale_storage_from_dense_template(const DENSE_STORAGE* rhs, dtype_t l_dtype) {
  LIType pos = 0, ndnz = 0;

  RDType R_ZERO; // need zero for easier comparisons
  if (rhs->dtype == RUBYOBJ)  R_ZERO = INT2FIX(0);
  else                        R_ZERO = 0;

  if (rhs->rank != 2) rb_raise(nm_eStorageTypeError, "can only convert matrices of rank 2 to yale");

  RDType* rhs_elements = reinterpret_cast<RDType*>(rhs->elements);

  // First, count the non-diagonal nonzeros
	for (size_t i = rhs->shape[0]; i-- > 0;) {
		for (size_t j = rhs->shape[1]; j-- > 0;) {
		  if (i != j && rhs_elements[pos] != R_ZERO)	++ndnz;

      // move forward 1 position in dense matrix elements array
      ++pos;
    }
  }

  // Copy shape for yale construction
  size_t* shape = ALLOC_N(size_t, 2);
  shape[0] = rhs->shape[0];
  shape[1] = rhs->shape[1];

  // Create with minimum possible capacity -- just enough to hold all of the entries
  YALE_STORAGE* lhs = yale_storage_create(l_dtype, shape, 2, shape[0] + ndnz + 1);
  LDType* lhs_a     = reinterpret_cast<LDType*>(lhs->a);
  LIType* lhs_ija   = reinterpret_cast<LIType*>(lhs->ija);

  // Set the zero position in the yale matrix
  lhs_a[ shape[0] ] = R_ZERO;

  // Start just after the zero position.
  LIType ija = lhs->shape[0]+1;
  LIType i;
  pos        = 0;

  // Copy contents
  for (i = 0; i < rhs->shape[0]; ++i) {
    // indicate the beginning of a row in the IJA array
    lhs_ija[i] = ija;

    for (LIType j = 0; j < rhs->shape[1]; ++j) {

      if (i == j) { // copy to diagonal
        lhs_a[i] = rhs_elements[pos];
      } else if (rhs_elements[pos] != R_ZERO) { // copy nonzero to LU
        lhs_ija[ija] = j; // write column index

        lhs_a[ija] = rhs_elements[pos];

        ++ija;
      }
      ++pos;
    }
  }
  lhs_ija[i] = ija; // indicate the end of the last row

  lhs->ndnz = ndnz;

  return lhs;
}

STORAGE* yale_storage_from_dense(const STORAGE* right, dtype_t l_dtype) {
	NAMED_LRI_DTYPE_TEMPLATE_TABLE(ttable, yale_storage_from_dense_template, YALE_STORAGE*, const DENSE_STORAGE* rhs, dtype_t l_dtype);

  itype_t itype = yale_storage_itype((const YALE_STORAGE*)right);

	return (STORAGE*)ttable[l_dtype][right->dtype][itype]((const DENSE_STORAGE*)right, l_dtype);
}


/*
 * Creation of yale storage from list storage.
 */
template <typename LDType, typename RDType, typename LIType>
YALE_STORAGE* yale_storage_from_list_template(const LIST_STORAGE* rhs, dtype_t l_dtype) {
  NODE *i_curr, *j_curr;
  size_t ndnz = list_storage_count_nd_elements(rhs);

  if (rhs->rank != 2) rb_raise(nm_eStorageTypeError, "can only convert matrices of rank 2 to yale");

  if ((rhs->dtype == RUBYOBJ && *reinterpret_cast<RubyObject*>(rhs->default_val) == INT2FIX(0))
      || strncmp(reinterpret_cast<const char*>(rhs->default_val), "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", DTYPE_SIZES[rhs->dtype]))
    rb_raise(nm_eStorageTypeError, "list matrix must have default value of 0 to convert to yale");


  // Copy shape for yale construction
  size_t* shape = ALLOC_N(size_t, 2);
  shape[0] = rhs->shape[0];
  shape[1] = rhs->shape[1];

  YALE_STORAGE* lhs = yale_storage_create(l_dtype, shape, 2, shape[0] + ndnz + 1);
  yale_storage_clear_diagonal_and_zero_template<LIType>(lhs); // clear the diagonal and the zero location.
  LIType* lhs_ija = reinterpret_cast<LIType*>(lhs->ija);
  LDType* lhs_a   = reinterpret_cast<LDType*>(lhs->a);

  LIType ija = lhs->shape[0]+1;

  for (i_curr = rhs->rows->first; i_curr; i_curr = i_curr->next) {

    // indicate the beginning of a row in the IJA array
    lhs_ija[i_curr->key] = ija;

    for (j_curr = ((LIST*)(i_curr->val))->first; j_curr; j_curr = j_curr->next) {
      LDType cast_jcurr_val = *reinterpret_cast<RDType*>(j_curr->val);

      if (i_curr->key == j_curr->key)
        lhs_a[i_curr->key] = cast_jcurr_val; // set diagonal
      else {

        lhs_ija[ija] = j_curr->key;    // set column value
        lhs_a[ija]   = cast_jcurr_val;                      // set cell value

        ++ija;
      }
    }

    if (!i_curr->next)	lhs_ija[i_curr->key] = ija; // indicate the end of the last row
  }

  lhs->ndnz = ndnz;
  return lhs;
}

STORAGE* yale_storage_from_list(const STORAGE* right, dtype_t l_dtype) {
	NAMED_LRI_DTYPE_TEMPLATE_TABLE(ttable, yale_storage_from_list_template, YALE_STORAGE*, const LIST_STORAGE* rhs, dtype_t l_dtype);

  itype_t itype = yale_storage_itype((const YALE_STORAGE*)right);

	return (STORAGE*)ttable[l_dtype][right->dtype][itype]((const LIST_STORAGE*)right, l_dtype);
}

//////////////////////
// Helper Functions //
//////////////////////

/*
 * Copy list contents into dense recursively.
 */
template <typename LDType, typename RDType>
static void dense_storage_cast_copy_list_contents_template(LDType* lhs, const LIST* rhs, RDType* default_val, size_t& pos, const size_t* shape, size_t rank, size_t max_elements, size_t recursions) {

  NODE *curr = rhs->first;
  int last_key = -1;

	for (size_t i = 0; i < shape[rank - 1 - recursions]; ++i, ++pos) {

    if (!curr || (curr->key > (size_t)(last_key+1))) {
      
      if (recursions == 0)  lhs[pos] = *default_val;
      else               		dense_storage_cast_copy_list_default_template<LDType,RDType>(lhs, default_val, pos, shape, rank, max_elements, recursions-1);

      ++last_key;
      
    } else {

      if (recursions == 0)  lhs[pos] = *reinterpret_cast<RDType*>(curr->val);
      else                	dense_storage_cast_copy_list_contents_template<LDType,RDType>(lhs, (const LIST*)(curr->val),
                                                                                         default_val, pos, shape, rank, max_elements, recursions-1);

      last_key = curr->key;
      curr     = curr->next;
    }
  }
  
  --pos;
}

/*
 * Copy a set of default values into dense.
 */
template <typename LDType,typename RDType>
static void dense_storage_cast_copy_list_default_template(LDType* lhs, RDType* default_val, size_t& pos, const size_t* shape, size_t rank, size_t max_elements, size_t recursions) {
	for (size_t i = 0; i < shape[rank - 1 - recursions]; ++i, ++pos) {

    if (recursions == 0)    lhs[pos] = *default_val;
    else                  	dense_storage_cast_copy_list_default_template<LDType,RDType>(lhs, default_val, pos, shape, rank, max_elements, recursions-1);

  }
  
  --pos;
}

