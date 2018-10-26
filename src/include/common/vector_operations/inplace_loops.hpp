//===----------------------------------------------------------------------===//
//
//                         DuckDB
//
// common/vector_operations/unary_loops.hpp
//
// Author: Mark Raasveldt
//
//===----------------------------------------------------------------------===//

#include "common/exception.hpp"
#include "common/types/vector.hpp"

namespace duckdb {

inline void INPLACE_TYPE_CHECK(Vector &left, Vector &result) {
	if (left.type != result.type) {
		throw TypeMismatchException(left.type, result.type,
		                            "input and result types must be the same");
	}
	if (!left.IsConstant() && left.count != result.count) {
		throw Exception("Cardinality exception: left and result cannot have "
		                "different cardinalities");
	}
}

template <class LEFT_TYPE, class RESULT_TYPE, class OP>
static inline void
inplace_loop_function_constant(LEFT_TYPE ldata,
                               RESULT_TYPE *__restrict result_data,
                               size_t count, sel_t *__restrict sel_vector) {
	if (sel_vector) {
		for (size_t i = 0; i < count; i++) {
			OP::Operation(result_data[sel_vector[i]], ldata);
		}
	} else {
		for (size_t i = 0; i < count; i++) {
			OP::Operation(result_data[i], ldata);
		}
	}
}

template <class LEFT_TYPE, class RESULT_TYPE, class OP>
static inline void
inplace_loop_function_array(LEFT_TYPE *__restrict ldata,
                            RESULT_TYPE *__restrict result_data, size_t count,
                            sel_t *__restrict sel_vector) {
	ASSERT_RESTRICT(ldata, result_data, count);
	if (sel_vector) {
		for (size_t i = 0; i < count; i++) {
			OP::Operation(result_data[sel_vector[i]], ldata[sel_vector[i]]);
		}
	} else {
		for (size_t i = 0; i < count; i++) {
			OP::Operation(result_data[i], ldata[i]);
		}
	}
}

template <class LEFT_TYPE, class RESULT_TYPE, class OP>
void templated_inplace_loop(Vector &left, Vector &result) {
	auto ldata = (LEFT_TYPE *)left.data;
	auto result_data = (RESULT_TYPE *)result.data;
	if (left.IsConstant()) {
		LEFT_TYPE constant = ldata[0];
		inplace_loop_function_constant<LEFT_TYPE, RESULT_TYPE, OP>(
		    constant, result_data, result.count, result.sel_vector);
	} else {
		// OR nullmasks together
		result.nullmask = left.nullmask | result.nullmask;
		assert(result.sel_vector == left.sel_vector);
		inplace_loop_function_array<LEFT_TYPE, RESULT_TYPE, OP>(
		    ldata, result_data, left.count, left.sel_vector);
	}
}

} // namespace duckdb