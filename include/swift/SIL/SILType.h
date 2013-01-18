//===--- SILType.h - Defines the SILType type -------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the SILType class, which is used to refer to SIL
// representation types.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SIL_SILType_H
#define SWIFT_SIL_SILType_H

#include "swift/AST/Type.h"
#include "swift/AST/Types.h"
#include "llvm/ADT/PointerIntPair.h"

namespace swift {
  class ASTContext;
  class SILTypeInfo;
  class SILFunctionTypeInfo;
  class SILCompoundTypeInfo;
  
namespace Lowering {
  class TypeConverter;
}

/// SILType - A Swift type that has been lowered to a SIL representation type.
/// In addition to the Swift type system, SIL also has an "address" type that
/// can reference any Swift type (but cannot take the address of an address).
/// SIL also has the notion of "loadable" vs "address-only" types: loadable
/// types have a fixed size and compile-time binary representation and thus
/// can be loaded from memory and represented as rvalues, whereas address-only
/// types do not have a known size or layout and must always be handled
/// indirectly in memory.
class SILType {
  typedef llvm::PointerIntPair<Type, 2> ValueType;
  ValueType value;
  
  enum {
    // Set if this is an address type.
    IsAddressFlag = 1 << 0,
    // Set if the underlying type is loadable. !IsAddress && !IsLoadable is
    // invalid.
    IsLoadableFlag = 1 << 1,
  };
  
  /// Private constructor. SILTypes are normally vended by
  /// TypeInfo::getLoweredType() in SILGen.
  SILType(CanType ty,
          bool address,
          bool loadable)
    : value(ty.getPointer(),
            (address << 0) | (loadable << 1)) {
    assert((address || loadable) &&
           "SILType can't be the value of an address-only type");
    assert(!(ty && ty->is<LValueType>()) &&
           "LValueTypes should be eliminated by SIL lowering");
  }
  
  SILType(Type ty, unsigned flags)
    : value(ty, flags) {}
  explicit SILType(ValueType value)
    : value(value) {}
  
  friend class Lowering::TypeConverter;
  friend class llvm::PointerLikeTypeTraits<SILType>;
public:
  SILType() = default;
  
  bool isNull() const { return value.getPointer().isNull(); }
  explicit operator bool() const { return value.getPointer().operator bool(); }
  
  void dump() const;
  void print(raw_ostream &OS) const;
  
  // Direct comparison is allowed for SILTypes because they are canonicalized.
  bool operator==(SILType T) const {
    return value == T.value;
  }
  bool operator!=(SILType T) const {
    return value != T.value;
  }
  
  /// Gets the address type referencing this type, or the type itself if it is
  /// already an address type.
  SILType getAddressType() const {
    return SILType(value.getPointer(), value.getInt() | IsAddressFlag);
  }
  /// Gets the type referenced by an address type, or the type itself if it is
  /// not an address type. Invalid for address-only types.
  SILType getObjectType() const {
    assert((value.getInt() | IsLoadableFlag) &&
           "dereferencing an address-only address");
    return SILType(value.getPointer(), value.getInt() & ~IsAddressFlag);
  }
  
  /// Returns the Swift type referenced by this SIL type.
  CanType getSwiftRValueType() const { return CanType(value.getPointer()); }
  /// Returns the Swift type equivalent to this SIL type. If the SIL type is
  /// an address type, returns an LValueType.
  CanType getSwiftType() const {
    if (isAddress()) {
      return LValueType::get(value.getPointer(),
                             LValueType::Qual::DefaultForType,
                             value.getPointer()->getASTContext())
              ->getCanonicalType();
    } else
      return CanType(value.getPointer());
  }
  
  /// Cast the Swift type referenced by this SIL type, or return null if the
  /// cast fails.
  template<typename TYPE>
  TYPE *getAs() const { return value.getPointer()->getAs<TYPE>(); }
  /// Cast the Swift type referenced by this SIL type, which must be of the
  /// specified subtype.
  template<typename TYPE>
  TYPE *castTo() const { return value.getPointer()->castTo<TYPE>(); }
  /// Returns true if the Swift type referenced by this SIL type is of the
  /// specified subtype.
  template<typename TYPE>
  bool is() const { return value.getPointer()->is<TYPE>(); }
  
  /// True if the type is an address type.
  bool isAddress() const { return value.getInt() & IsAddressFlag; }
  /// True if the type, or the referenced type of an address type, is loadable.
  /// This is the opposite of isAddressOnly.
  bool isLoadable() const { return value.getInt() & IsLoadableFlag; }
  /// True if the type, or the referenced type of an address type, is
  /// address-only. This is the opposite of isLoadable.
  bool isAddressOnly() const { return !isLoadable(); }

  /// Returns true if the referenced type has reference semantics.
  bool hasReferenceSemantics() const {
    return value.getPointer()->hasReferenceSemantics();
  }
  /// Returns true if the referenced type is an existential type.
  bool isExistentialType() const {
    return value.getPointer()->isExistentialType();
  }
  
  /// Returns the ASTContext for the referenced Swift type.
  ASTContext &getASTContext() const {
    return value.getPointer()->getASTContext();
  }
  
  /// Get a SILType from a Swift Type that has already been lowered. This is
  /// dangerous. User code should instead use SILGen's
  /// TypeInfo::getLoweredType().
  static SILType getPreLoweredType(TypeBase *t,
                                   bool address,
                                   bool loadable) {
    return SILType(CanType(t), address, loadable);
  }

  static SILType getPreLoweredType(Type t,
                                   bool address,
                                   bool loadable) {
    return SILType(CanType(t), address, loadable);
  }
  
  //
  // Accessors for types used in SIL instructions:
  //
  
  /// Get the empty tuple type as a SILType.
  static SILType getEmptyTupleType(ASTContext &C);
  /// Get the ObjectPointer type as a SILType.
  static SILType getObjectPointerType(ASTContext &C);
  /// Get the RawPointer type as a SILType.
  static SILType getRawPointerType(ASTContext &C);
  /// Get the OpaquePointer type as a SILType.
  static SILType getOpaquePointerType(ASTContext &C);
  /// Get a builtin integer type as a SILType.
  static SILType getBuiltinIntegerType(unsigned bitWidth, ASTContext &C);
  
  //
  // Utilities for treating SILType as a pointer-like type.
  //
  void *getOpaqueValue() const { return value.getOpaqueValue(); }
  static SILType getFromOpaqueValue(void *P) {
    return SILType(ValueType::getFromOpaqueValue(P));
  }
};

} // end swift namespace

namespace llvm {

// Allow SILType to be treated as a pointer-like type.
template<>
class PointerLikeTypeTraits<swift::SILType> {
public:
  static inline swift::SILType getFromVoidPointer(void *P) {
    return swift::SILType::getFromOpaqueValue(P);
  }

  static inline void *getAsVoidPointer(swift::SILType T) {
    return T.getOpaqueValue();
  }
  
  enum { NumLowBitsAvailable = 1 };
};

// Allow SILType to be used as a DenseMap key.
template<>
class DenseMapInfo<swift::SILType> {
  using SILType = swift::SILType;
  using PointerMapInfo = DenseMapInfo<void*>;
public:
  static SILType getEmptyKey() {
    return SILType::getFromOpaqueValue(PointerMapInfo::getEmptyKey());
  }
  static SILType getTombstoneKey() {
    return SILType::getFromOpaqueValue(PointerMapInfo::getTombstoneKey());
  }
  static unsigned getHashValue(SILType t) {
    return PointerMapInfo::getHashValue(t.getOpaqueValue());
  }
  static bool isEqual(SILType LHS, SILType RHS) {
    return LHS == RHS;
  }
};

} // end llvm namespace


#endif
