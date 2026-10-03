//
//  ParamAddressMacros.h
//  Vendored from PolygraphModular (AUApr25) — unmodified. Pure macros, no platform types.
//
//  A module parameter address is a 24-bit packed value: (kind<<16)|(instance<<8)|item.
//  This drops directly into a CLAP clap_id (uint32) with no remapping.
//

#ifndef ParamAddressMacros_h
#define ParamAddressMacros_h

#define MODULE_PARAM_ADDRESS(kind, instance, item)  ((((kind) & 0xff) << 16) \
                                                | (((instance) & 0xff) << 8) \
                                                | ((item) & 0xff))

#define MODULE_PARAM_INSTANCE_BASE(kind, instance)  ((((kind) & 0xff) << 16) \
                                                | (((instance) & 0xff) << 8))

#define MODULE_PARAM_FROM_BASE_AND_ITEM(base, item) (((base) & 0x00ffff00) \
                                                |  ((item) & 0xff))

#define MODULE_KIND_FROM_ADDRESS(addr)        ((0x00ff0000 & (addr)) >> 16)
#define MODULE_INSTANCE_FROM_ADDRESS(addr)    ((0x0000ff00 & (addr)) >> 8)
#define MODULE_ITEM_FROM_ADDRESS(addr)        (0x000000ff & (addr))

#define MODULE_BASE_ADDRESS(addr)             (0xffffff00 & (addr))

#endif /* ParamAddressMacros_h */
