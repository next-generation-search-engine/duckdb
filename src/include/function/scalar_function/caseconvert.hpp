#pragma once
#include "common/types/data_chunk.hpp"
#include "function/function.hpp"

namespace duckdb {
namespace function {

void upper_function(Vector inputs[], size_t input_count, Vector &result);
void lower_function(Vector inputs[], size_t input_count, Vector &result);

bool caseconvert_matches_arguments(std::vector<TypeId> &arguments);
TypeId caseconvert_get_return_type(std::vector<TypeId> &arguments);

class UpperFunction {
  public:
	static const char *GetName() {
		return "upper";
	}

	static scalar_function_t GetFunction() {
		return upper_function;
	}

	static matches_argument_function_t GetMatchesArgumentFunction() {
		return caseconvert_matches_arguments;
	}

	static get_return_type_function_t GetReturnTypeFunction() {
		return caseconvert_get_return_type;
	}
};

class LowerFunction {
  public:
	static const char *GetName() {
		return "lower";
	}

	static scalar_function_t GetFunction() {
		return lower_function;
	}

	static matches_argument_function_t GetMatchesArgumentFunction() {
		return caseconvert_matches_arguments;
	}

	static get_return_type_function_t GetReturnTypeFunction() {
		return caseconvert_get_return_type;
	}
};

} // namespace function
} // namespace duckdb