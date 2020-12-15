// Autogenerated by the ProtoZero compiler plugin. DO NOT EDIT.

#ifndef PERFETTO_PROTOS_COUNTER_DESCRIPTOR_PROTO_H_
#define PERFETTO_PROTOS_COUNTER_DESCRIPTOR_PROTO_H_

#include <stddef.h>
#include <stdint.h>

#include "perfetto/protozero/message.h"
#include "perfetto/protozero/packed_repeated_fields.h"
#include "perfetto/protozero/proto_decoder.h"
#include "perfetto/protozero/proto_utils.h"

namespace perfetto {
namespace protos {
namespace pbzero {

enum CounterDescriptor_BuiltinCounterType : int32_t;
enum CounterDescriptor_Unit : int32_t;

enum CounterDescriptor_BuiltinCounterType : int32_t {
  CounterDescriptor_BuiltinCounterType_COUNTER_UNSPECIFIED = 0,
  CounterDescriptor_BuiltinCounterType_COUNTER_THREAD_TIME_NS = 1,
  CounterDescriptor_BuiltinCounterType_COUNTER_THREAD_INSTRUCTION_COUNT = 2,
};

const CounterDescriptor_BuiltinCounterType CounterDescriptor_BuiltinCounterType_MIN = CounterDescriptor_BuiltinCounterType_COUNTER_UNSPECIFIED;
const CounterDescriptor_BuiltinCounterType CounterDescriptor_BuiltinCounterType_MAX = CounterDescriptor_BuiltinCounterType_COUNTER_THREAD_INSTRUCTION_COUNT;

enum CounterDescriptor_Unit : int32_t {
  CounterDescriptor_Unit_UNIT_UNSPECIFIED = 0,
  CounterDescriptor_Unit_UNIT_TIME_NS = 1,
  CounterDescriptor_Unit_UNIT_COUNT = 2,
  CounterDescriptor_Unit_UNIT_SIZE_BYTES = 3,
};

const CounterDescriptor_Unit CounterDescriptor_Unit_MIN = CounterDescriptor_Unit_UNIT_UNSPECIFIED;
const CounterDescriptor_Unit CounterDescriptor_Unit_MAX = CounterDescriptor_Unit_UNIT_SIZE_BYTES;

class CounterDescriptor_Decoder : public ::protozero::TypedProtoDecoder</*MAX_FIELD_ID=*/5, /*HAS_NONPACKED_REPEATED_FIELDS=*/true> {
 public:
  CounterDescriptor_Decoder(const uint8_t* data, size_t len) : TypedProtoDecoder(data, len) {}
  explicit CounterDescriptor_Decoder(const std::string& raw) : TypedProtoDecoder(reinterpret_cast<const uint8_t*>(raw.data()), raw.size()) {}
  explicit CounterDescriptor_Decoder(const ::protozero::ConstBytes& raw) : TypedProtoDecoder(raw.data, raw.size) {}
  bool has_type() const { return at<1>().valid(); }
  int32_t type() const { return at<1>().as_int32(); }
  bool has_categories() const { return at<2>().valid(); }
  ::protozero::RepeatedFieldIterator<::protozero::ConstChars> categories() const { return GetRepeated<::protozero::ConstChars>(2); }
  bool has_unit() const { return at<3>().valid(); }
  int32_t unit() const { return at<3>().as_int32(); }
  bool has_unit_multiplier() const { return at<4>().valid(); }
  int64_t unit_multiplier() const { return at<4>().as_int64(); }
  bool has_is_incremental() const { return at<5>().valid(); }
  bool is_incremental() const { return at<5>().as_bool(); }
};

class CounterDescriptor : public ::protozero::Message {
 public:
  using Decoder = CounterDescriptor_Decoder;
  enum : int32_t {
    kTypeFieldNumber = 1,
    kCategoriesFieldNumber = 2,
    kUnitFieldNumber = 3,
    kUnitMultiplierFieldNumber = 4,
    kIsIncrementalFieldNumber = 5,
  };
  using BuiltinCounterType = ::perfetto::protos::pbzero::CounterDescriptor_BuiltinCounterType;
  using Unit = ::perfetto::protos::pbzero::CounterDescriptor_Unit;
  static const BuiltinCounterType COUNTER_UNSPECIFIED = CounterDescriptor_BuiltinCounterType_COUNTER_UNSPECIFIED;
  static const BuiltinCounterType COUNTER_THREAD_TIME_NS = CounterDescriptor_BuiltinCounterType_COUNTER_THREAD_TIME_NS;
  static const BuiltinCounterType COUNTER_THREAD_INSTRUCTION_COUNT = CounterDescriptor_BuiltinCounterType_COUNTER_THREAD_INSTRUCTION_COUNT;
  static const Unit UNIT_UNSPECIFIED = CounterDescriptor_Unit_UNIT_UNSPECIFIED;
  static const Unit UNIT_TIME_NS = CounterDescriptor_Unit_UNIT_TIME_NS;
  static const Unit UNIT_COUNT = CounterDescriptor_Unit_UNIT_COUNT;
  static const Unit UNIT_SIZE_BYTES = CounterDescriptor_Unit_UNIT_SIZE_BYTES;
  void set_type(::perfetto::protos::pbzero::CounterDescriptor_BuiltinCounterType value) {
    AppendTinyVarInt(1, value);
  }
  void add_categories(const std::string& value) {
    AppendBytes(2, value.data(), value.size());
  }
  void add_categories(const char* data, size_t size) {
    AppendBytes(2, data, size);
  }
  void set_unit(::perfetto::protos::pbzero::CounterDescriptor_Unit value) {
    AppendTinyVarInt(3, value);
  }
  void set_unit_multiplier(int64_t value) {
    AppendVarInt(4, value);
  }
  void set_is_incremental(bool value) {
    AppendTinyVarInt(5, value);
  }
};

} // Namespace.
} // Namespace.
} // Namespace.
#endif  // Include guard.
