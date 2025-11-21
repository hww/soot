
// Define a message as an enumeration.
#ifndef REGISTER_NAME
#define REGISTER_NAME(name)	name,
#define REGISTERING_ENUM
enum class EOpcode : u8 {
#endif

	/** Execution */
	REGISTER_NAME(Return)
	REGISTER_NAME(Move)
	REGISTER_NAME(Call)
	REGISTER_NAME(CallNat)
	REGISTER_NAME(Branch)
	REGISTER_NAME(BranchIf)
	REGISTER_NAME(BranchIfNot)
	/** The integer values operations */
	REGISTER_NAME(AddInt)
	REGISTER_NAME(SubInt)
	REGISTER_NAME(MulInt)
	REGISTER_NAME(DivInt)
	REGISTER_NAME(ModInt)
	REGISTER_NAME(AbsInt)
	REGISTER_NAME(NegFloat)
	REGISTER_NAME(NegInt)
        REGISTER_NAME(AshInt)
        REGISTER_NAME(ToInt)
	/** The integer operations with immediate argument */
	REGISTER_NAME(LoadImediateInt)
	REGISTER_NAME(AddImm)
	REGISTER_NAME(SubImm)
	REGISTER_NAME(MulImm)
	REGISTER_NAME(DivImm)
	/** The floating point values operations */
	REGISTER_NAME(AddFloat)
	REGISTER_NAME(SubFloat)
	REGISTER_NAME(MulFloat)
	REGISTER_NAME(DivFloat)
	REGISTER_NAME(ModFloat)
	REGISTER_NAME(AbsFloat)
	REGISTER_NAME(ToFloat)
	/** Comparisong */
	REGISTER_NAME(CmpEqual)
	REGISTER_NAME(CmpGt)
	REGISTER_NAME(CmpGtEqual)
	REGISTER_NAME(CmpLt)
	REGISTER_NAME(CmpLtEqual)
	REGISTER_NAME(CmpFloatEqual)
	REGISTER_NAME(CmpFloatGt)
	REGISTER_NAME(CmpFloatGtEqual)
	REGISTER_NAME(CmpFloatLt)
	REGISTER_NAME(CmpFloatLtEqual)
	/** Logical operations */
	REGISTER_NAME(LogAnd)
	REGISTER_NAME(LogOr)
	REGISTER_NAME(LogNot)
	/** Bitwise operations */
	REGISTER_NAME(BitAnd)
	REGISTER_NAME(BitNot)
	REGISTER_NAME(BitOr)
	REGISTER_NAME(BitXor)
	REGISTER_NAME(BitNor)
	/** Utilities */
	REGISTER_NAME(LocadArgc)
	REGISTER_NAME(GetSidStr)
	/** Lookup object */
	REGISTER_NAME(LookupInt)
	REGISTER_NAME(LookupFloat)
	REGISTER_NAME(LookupPointer)
	/** Load indirect by the pointer */
	REGISTER_NAME(LoadIndInt)
	REGISTER_NAME(LoadIndFloat)
	REGISTER_NAME(LoadIndPointer)
	/** Store indirect by the pointer */
	REGISTER_NAME(StoreIndInt)
	REGISTER_NAME(StoreIndFloat)
	REGISTER_NAME(StoreIndPointer)
	/** Load static values the values are located in the data[] array */
	REGISTER_NAME(LoadStaticInt)
	REGISTER_NAME(LoadStaticFloat)
	REGISTER_NAME(LoadStaticPointer)

#ifdef REGISTERING_ENUM
};
#undef REGISTER_NAME
#undef REGISTERING_ENUM
#endif
