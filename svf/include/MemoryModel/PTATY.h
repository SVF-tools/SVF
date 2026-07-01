#ifndef PTATY_H
#define PTATY_H

namespace SVF
{

/// Pointer analysis type list
enum PTATY
{
    // Whole program analysis
    Andersen_BASE,		///< Base Andersen PTA
    Andersen_WPA,		///< Andersen PTA
    AndersenSCD_WPA,    ///< Selective cycle detection andersen-style WPA
    AndersenSFR_WPA,    ///< Stride-based field representation
    AndersenWaveDiff_WPA,	///< Diff wave propagation andersen-style WPA
    Steensgaard_WPA,      ///< Steensgaard PTA
    CSCallString_WPA,	///< Call string based context sensitive WPA
    CSSummary_WPA,		///< Summary based context sensitive WPA
    FSDATAFLOW_WPA,	///< Traditional Dataflow-based flow sensitive WPA
    FSSPARSE_WPA,		///< Sparse flow sensitive WPA
    VFS_WPA,		///< Versioned sparse flow-sensitive WPA
    FSCS_WPA,			///< Flow-, context- sensitive WPA
    CFLFICI_WPA,		///< Flow-, context-, insensitive CFL-reachability-based analysis
    CFLFSCI_WPA,		///< Flow-insensitive, context-sensitive CFL-reachability-based analysis
    CFLFSCS_WPA,	///< Flow-, context-, CFL-reachability-based analysis
    TypeCPP_WPA, ///<  Type-based analysis for C++

    // Demand driven analysis
    FieldS_DDA,		///< Field sensitive DDA
    FlowS_DDA,		///< Flow sensitive DDA
    PathS_DDA,		///< Guarded value-flow DDA
    Cxt_DDA,		///< context sensitive DDA


    Default_PTA		///< default pta without any analysis
};

/// Implementation type: BVDataPTAImpl or CondPTAImpl.
enum PTAImplTy
{
    BaseImpl,   ///< Represents PointerAnalaysis.
    BVDataImpl, ///< Represents BVDataPTAImpl.
    CondImpl,   ///< Represents CondPTAImpl.
};

/// How the PTData used is implemented.
enum PTBackingType
{
    Mutable,
    Persistent,
};

}  // namespace SVF

#endif // PTATY_H
