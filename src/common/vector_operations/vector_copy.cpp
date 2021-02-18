//===--------------------------------------------------------------------===//
// copy.cpp
// Description: This file contains the implementation of the different copy
// functions
//===--------------------------------------------------------------------===//

#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/null_value.hpp"
#include "duckdb/common/types/chunk_collection.hpp"

#include "duckdb/common/vector_operations/vector_operations.hpp"

namespace duckdb {

template <class T>
static void TemplatedCopy(Vector &source, const SelectionVector &sel, Vector &target, idx_t source_offset,
                          idx_t target_offset, idx_t copy_count) {
	auto ldata = FlatVector::GetData<T>(source);
	auto tdata = FlatVector::GetData<T>(target);
	for (idx_t i = 0; i < copy_count; i++) {
		auto source_idx = sel.get_index(source_offset + i);
		tdata[target_offset + i] = ldata[source_idx];
	}
}

void VectorOperations::Copy(Vector &source, Vector &target, const SelectionVector &sel, idx_t source_count,
                            idx_t source_offset, idx_t target_offset) {
	D_ASSERT(source_offset <= source_count);
	D_ASSERT(target.GetVectorType() == VectorType::FLAT_VECTOR);
	D_ASSERT(source.GetType() == target.GetType());
	switch (source.GetVectorType()) {
	case VectorType::DICTIONARY_VECTOR: {
		// dictionary vector: merge selection vectors
		auto &child = DictionaryVector::Child(source);
		auto &dict_sel = DictionaryVector::SelVector(source);
		// merge the selection vectors and verify the child
		auto new_buffer = dict_sel.Slice(sel, source_count);
		SelectionVector merged_sel(new_buffer);
		VectorOperations::Copy(child, target, merged_sel, source_count, source_offset, target_offset);
		return;
	}
	case VectorType::SEQUENCE_VECTOR: {
		int64_t start, increment;
		Vector seq(source.GetType());
		SequenceVector::GetSequence(source, start, increment);
		VectorOperations::GenerateSequence(seq, source_count, sel, start, increment);
		VectorOperations::Copy(seq, target, sel, source_count, source_offset, target_offset);
		return;
	}
	case VectorType::CONSTANT_VECTOR:
	case VectorType::FLAT_VECTOR:
		break;
	default:
		throw NotImplementedException("FIXME unimplemented vector type for VectorOperations::Copy");
	}

	idx_t copy_count = source_count - source_offset;
	D_ASSERT(target_offset + copy_count <= STANDARD_VECTOR_SIZE);
	if (copy_count == 0) {
		return;
	}

	// first copy the nullmask
	auto &tmask = FlatVector::Validity(target);
	if (source.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		if (ConstantVector::IsNull(source)) {
			for (idx_t i = 0; i < copy_count; i++) {
				tmask.SetInvalid(target_offset + i);
			}
		}
	} else {
		auto &smask = FlatVector::Validity(source);
		for (idx_t i = 0; i < copy_count; i++) {
			auto idx = sel.get_index(source_offset + i);
			tmask.Set(target_offset + i, smask.RowIsValid(idx));
		}
	}

	// now copy over the data
	switch (source.GetType().InternalType()) {
	case PhysicalType::BOOL:
	case PhysicalType::INT8:
		TemplatedCopy<int8_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::INT16:
		TemplatedCopy<int16_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::INT32:
		TemplatedCopy<int32_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::HASH:
	case PhysicalType::INT64:
		TemplatedCopy<int64_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::UINT8:
		TemplatedCopy<uint8_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::UINT16:
		TemplatedCopy<uint16_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::UINT32:
		TemplatedCopy<uint32_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::UINT64:
		TemplatedCopy<uint64_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::INT128:
		TemplatedCopy<hugeint_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::POINTER:
		TemplatedCopy<uintptr_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::FLOAT:
		TemplatedCopy<float>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::DOUBLE:
		TemplatedCopy<double>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::INTERVAL:
		TemplatedCopy<interval_t>(source, sel, target, source_offset, target_offset, copy_count);
		break;
	case PhysicalType::VARCHAR: {
		auto ldata = FlatVector::GetData<string_t>(source);
		auto tdata = FlatVector::GetData<string_t>(target);
		for (idx_t i = 0; i < copy_count; i++) {
			auto source_idx = sel.get_index(source_offset + i);
			auto target_idx = target_offset + i;
			if (tmask.RowIsValid(target_idx)) {
				tdata[target_idx] = StringVector::AddStringOrBlob(target, ldata[source_idx]);
			}
		}
		break;
	}
	case PhysicalType::STRUCT: {
		if (StructVector::HasEntries(target)) {
			// target already has entries: append to them
			auto &source_children = StructVector::GetEntries(source);
			auto &target_children = StructVector::GetEntries(target);
			D_ASSERT(source_children.size() == target_children.size());
			for (idx_t i = 0; i < source_children.size(); i++) {
				D_ASSERT(target_children[i].first == target_children[i].first);
				VectorOperations::Copy(*source_children[i].second, *target_children[i].second, sel, source_count,
				                       source_offset, target_offset);
			}
		} else {
			D_ASSERT(target_offset == 0);
			// target has no entries: create new entries for the target
			auto &source_children = StructVector::GetEntries(source);
			for (auto &child : source_children) {
				auto child_copy = make_unique<Vector>(child.second->GetType());

				VectorOperations::Copy(*child.second, *child_copy, sel, source_count, source_offset, target_offset);
				StructVector::AddEntry(target, child.first, move(child_copy));
			}
		}
		break;
	}
	case PhysicalType::LIST: {
		D_ASSERT(target.GetType().InternalType() == PhysicalType::LIST);
		if (ListVector::HasEntry(source)) {
			// if the source has list offsets, we need to append them to the target
			if (!ListVector::HasEntry(target)) {
				auto new_target_child = make_unique<ChunkCollection>();
				ListVector::SetEntry(target, move(new_target_child));
			}
			auto &source_child = ListVector::GetEntry(source);
			auto &target_child = ListVector::GetEntry(target);
			idx_t old_target_child_len = target_child.Count();

			// append to list itself
			target_child.Append(source_child);
			// now write the list offsets
			auto ldata = FlatVector::GetData<list_entry_t>(source);
			auto tdata = FlatVector::GetData<list_entry_t>(target);
			for (idx_t i = 0; i < copy_count; i++) {
				auto source_idx = sel.get_index(source_offset + i);
				auto &source_entry = ldata[source_idx];
				auto &target_entry = tdata[target_offset + i];

				target_entry.length = source_entry.length;
				target_entry.offset = old_target_child_len + source_entry.offset;
			}
		}
		break;
	}
	default:
		throw NotImplementedException("Unimplemented type '%s' for copy!",
		                              TypeIdToString(source.GetType().InternalType()));
	}
}

void VectorOperations::Copy(Vector &source, Vector &target, idx_t source_count, idx_t source_offset,
                            idx_t target_offset) {
	switch (source.GetVectorType()) {
	case VectorType::DICTIONARY_VECTOR: {
		// dictionary: continue into child with selection vector
		auto &child = DictionaryVector::Child(source);
		auto &dict_sel = DictionaryVector::SelVector(source);
		VectorOperations::Copy(child, target, dict_sel, source_count, source_offset, target_offset);
		break;
	}
	case VectorType::CONSTANT_VECTOR:
		VectorOperations::Copy(source, target, ConstantVector::ZERO_SELECTION_VECTOR, source_count, source_offset,
		                       target_offset);
		break;
	default:
		source.Normalify(source_count);
		VectorOperations::Copy(source, target, FlatVector::INCREMENTAL_SELECTION_VECTOR, source_count, source_offset,
		                       target_offset);
		break;
	}
}

} // namespace duckdb
