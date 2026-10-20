/// SA Message Management.
///
/// Manage output message descriptions class.
///
/// @file PlnSaMessage.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
using std::string;

enum PlnSaMessageCode {
	M_Help,
	M_Version,
	E_DuplicateVarDecl,		// arg1: variable name
	E_DuplicateFuncDef,		// arg1: function name
	E_IncompatibleTypeCast,	// arg1: from type, arg2: to type
	E_UndefinedFunction,	// arg1: function name
	E_InvalidNarrowingConv,	// arg1: source type name, arg2: target type name
	E_ExportInBlock,		// arg1: function name
	E_ExportInFunction,		// arg1: function name
	E_UndefinedVariable,	// arg1: variable name
	E_ReturnOutsideFunction,
	E_NamedRetBareReturn,
	E_SingleRetOneExpr,
	E_VoidBareReturn,
	E_VoidCallUsedAsValue,
	E_TupleUndefinedFunction,	// arg1: function name
	E_TupleNeedsMultiRet,		// arg1: function name
	E_TupleVarCountMismatch,	// arg1: function name
	E_ImportFileNotFound,		// arg1: file path
	E_NoInputFile,
	E_InternalError,			// arg1: error code
	E_BreakOutsideLoop,
	E_ContinueOutsideLoop,
	E_FloatModulo,
	E_ArraySizeNotInteger,
	E_ArrayIndexNotInteger,
	E_NotArrayType,
	E_UnsizedArrVarDecl,
	E_LogicalOpNotInteger,
	E_EmbeddedArrUnsizedInner,		// []$[]T in parameter: inner dimension must be fixed
	E_EmbeddedArrInnerSizeMismatch,	// arg1: expected inner-size, arg2: actual
	E_AmbiguousCall,			// arg1: function name
	E_UnknownAlias,				// arg1: alias name
	E_UnqualifiedAliasCall,			// arg1: function name
	E_UnknownType,				// arg1: type name
	E_UnknownField,				// arg1: struct name, arg2: field name
	E_UnsupportedStructFieldType,
	E_RecursiveStruct,			// arg1: struct name
	E_FieldAccessOnNonStruct,
	E_WriteToImmutablePtrField,
	E_EmbedOwningStruct,			// arg1: struct name
	E_WriteToReadOnlyArrElem,
	E_ArrFieldSizeNotConstant,	// arg1 (optional): the name that is not a const
	E_ConflictingTypedef,			// arg1: typedef name
	E_ConstNotCompileTimeValue,		// arg1: const name
	E_AddrOfNotLocalVar,			// arg1: variable name
	E_AddrOfNotPrimitive,			// arg1: variable or field name
	E_WriteThroughReadOnlyPtr,
	E_PtrMutabilityUpgrade,
	E_AssignToWholeStructElem,
	E_AddrOfNotAddressable,
	E_ReadOnlyPtrToNonConstCParam,		// arg1: C function name, arg2: parameter name
	E_AddrOfNotPrimitiveElem,
	E_BitwiseOpNotInteger,
	E_IncompleteStructType,		// arg1: struct name, arg2: incomplete reason ("unsupported-field" or "forward-declared")
	E_UnsupportedParamType,		// arg1: function name, arg2: unsupported type display name
	E_UnsupportedCFuncSignature,	// arg1: C function name, arg2: unsupported type display name
	E_CGlobalNotAssignable,		// arg1: C global variable name
	E_CGlobalNotAddressable,		// arg1: C global variable name
	E_UnsupportedCGlobalType,		// arg1: C global variable name, arg2: unsupported type display name
	E_ArithOpNotNumeric,
	E_StructInitNotSupported,	// arg1: struct type name
	E_IncompatibleTypes,		// arg1: from type display name, arg2: to type display name
	E_ByvalStructRetDiscarded,	// arg1: struct type name
	E_FuncNotValue,		// arg1: function name, arg2: the reason
	E_FuncPtrMutable,
	E_FuncPtrSigType,	// arg1: type display name
	E_AmbiguousFuncPtrCall,	// arg1: the name called
	E_DerefVoidPointer,		// no args
	E_InvalidLinkLibName,		// arg1: the library name as written
	E_LinkObjNotFound,			// arg1: the object file's absolute path
	E_SyscallNumberNotConstant,	// arg1: syscall name
	E_SyscallNumberOutOfRange,	// arg1: syscall name, arg2: the number as written
	E_SyscallTooManyParams,		// arg1: syscall name, arg2: parameter count
	E_SyscallUnsupportedParamType,	// arg1: syscall name, arg2: unsupported type display name
	E_VarTypeInferenceNotImpl,	// arg1: variable name
	E_StmtNotImplemented,
	E_ExprNotImplemented,
	E_ParamDefaultNotImpl,	// arg1: parameter name
	E_ParamNotImplemented,
	E_RetNotImplemented,
	E_IntLiteralOutOfRange,	// arg1: the literal as written, arg2: adopted type display name
	E_ArrLitContext,
	E_ArrLitCountMismatch,	// arg1: variable name, arg2: declared size, arg3: literal element count
	E_ArrLitSizeNotConst,	// arg1: variable name
	E_ArrLitDimMismatch,	// arg1: variable name
	E_ArrLitElemType,	// arg1: variable name
	E_ArrLitRowSizeMismatch,	// arg1: variable name, arg2: row size, arg3: the row's element count
	E_ArrBorrowSizeNotConst,
	E_ArrBorrowUnsupportedElem,
	E_ArrBorrowNeedsAddrOf,
	E_ArrBorrowShapeMismatch,	// arg1: expected shape, arg2: actual shape
	E_StructBorrowNeedsAddrOf,
	E_Unsupported2DStructArr,
	E_StructLitFieldCount,	// arg1: struct name, arg2: field count, arg3: value count
	E_StructLitDupField,	// arg1: field name
	E_StructLitMissingField,	// arg1: struct name, arg2: field name
	E_StructLitExpected,	// arg1: struct name
	E_StructLitFieldType,	// arg1: struct name, arg2: field name
	E_DictLitContext,
	E_ArrLitArgType,	// arg1: parameter name, arg2: parameter type
	E_DictLitArgType,	// arg1: parameter name, arg2: parameter type
	E_LitArgCFunc,	// arg1: C function name
	E_CopyShapeMismatch,	// arg1: destination shape, arg2: source shape
	E_CopyUnsupportedShape,	// arg1: shape
	E_AddrOfBorrowed,	// arg1: variable name
	E_EmbedElemNotStruct,	// arg1: element type
	E_NamedRetInitOnStruct,	// arg1: return name
	E_TransferToNonOwnedField,	// arg1: field name
	E_ExpiringStructToBorrow,
	E_TransferFromNonOwner,
	E_TransferToBorrowSlot,
	E_TransferShapeMismatch,	// arg1: destination shape, arg2: source shape
	E_TransferToNonOwnedVar,	// arg1: variable name
	E_TransferToSelf,	// arg1: variable name
	E_UnsizedArrRowSizeNotConst,
	E_UnsizedArrRowUnsupported,
	E_ArrElemShapeMismatch,	// arg1: expected shape, arg2: actual shape
	E_ArgCountMismatch,	// arg1: function name, arg2: expected count, arg3: actual count
	E_UnknownEnumerator,	// arg1: enumerator name, arg2: enum type name
	E_DuplicateEnumerator,	// arg1: enumerator name, arg2: enum type name
	E_EnumValueNotConst,	// arg1: enumerator name
	E_EnumValueOutOfRange,	// arg1: enumerator name, arg2: value
	E_DuplicateTypeName,	// arg1: type name
	E_DuplicateConstDecl,	// arg1: const name
	E_CastArgCount,	// arg1: target type name
	E_SizeofArrSizeNotConst,
	E_MethodNeedsMutableFirstParam,	// arg1: function name
	E_UnqualifiedAliasType,	// arg1: type name, arg2: module alias
	E_CircularTypeDecl,	// arg1: type name, arg2: dependency path
	E_InvalidCastTarget,	// arg1: target type name
	E_CastToOwnedArr,
	E_StructLitTarget,
	E_ArrLitTarget,	// arg1: variable name
	E_BorrowOfTempEscapes,
	E_UnownedCallResult,
	E_AssignInExpr,
	E_ChainToMethodCall,
	E_ChainMultiToTapple,
	E_UnsupportedCFieldType,	// arg1: struct name, arg2: field name, arg3: unsupported type display name
	E_PtrInnerPermissionNarrowing,	// arg1: from type display name, arg2: to type display name
	E_ConstExprOverflow,
	E_ConstExprDivByZero,
	E_CMacroFuncLike,	// arg1: macro name
	E_CMacroNotConstant,	// arg1: macro name
	E_FrameVarType,	// arg1: type display name
	E_FrameArrSizeNotConst,	// arg1: variable name
	E_ReturnFrameArr,	// arg1: variable name
};

class PlnSaMessage
{
public:
	static string getMessage(PlnSaMessageCode msg_code,
	                         string arg1="\x01", string arg2="\x01",
	                         string arg3="\x01");
	// The program-wide name of an unexported struct/enum, or one declared in
	// a block. A message shows it as written.
	static string localTypeName(const string& moduleId, int seq, const string& name);
private:
	static string format(PlnSaMessageCode msg_code, string arg1, string arg2, string arg3);
};
