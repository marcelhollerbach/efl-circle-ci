
#ifndef EFL_CXX_DETAIL_EO_OPS_HH
#define EFL_CXX_DETAIL_EO_OPS_HH

extern "C"
{
#include <Eo.h>
}

namespace efl { namespace eo { namespace detail {

inline void
init()
{
   ::eo_init();
}

inline void
shutdown()
{
   ::eo_shutdown();
}

inline Eo*
ref(Eo *obj)
{
   return ::eo_ref(obj);
}

inline void
unref(Eo *obj)
{
   ::eo_unref(obj);
}

inline int
ref_get(Eo *obj)
{
   return ::eo_ref_get(obj);
}

inline void
del(Eo *obj)
{
   ::eo_del(obj);
}

inline Eina_Bool
isa(Eo *obj, Eo_Class *klass)
{
   return eo_isa(obj, klass);
}

inline Eo*
add(Eo_Class *klass, Eo *parent = NULL)
{
   Eo *eo = nullptr;
   eo_add_ref(&eo, klass, parent);
   return eo;
}

inline void
dbg_info_get(Eo *obj, Eo_Dbg_Info *info)
{
   eo_dbg_info_get(obj, info);
}

inline void
base_data_set(Eo *obj, const char *key, const void *data)
{
   eo_key_data_set(obj, key, data);
}

inline void*
base_data_get(Eo *obj, const char *key)
{
   void *data;
   data = eo_key_data_get(obj, key);
   return data;
}

inline void
base_data_del(Eo *obj, const char *key)
{
   eo_key_data_del(obj, key);
}

inline void
parent_set(Eo *obj, Eo *parent)
{
   eo_parent_set(obj, parent);
}

inline Eo*
parent_get(Eo *obj)
{
   Eo *parent;
   parent = eo_parent_get(obj);
   return parent;
}

inline void
event_freeze(Eo *obj)
{
   eo_event_freeze(obj);
}

inline void
event_thaw(Eo *obj)
{
   eo_event_thaw(obj);
}

inline int
event_freeze_get(Eo *obj)
{
   int count = -1;
   count = eo_event_freeze_count_get(obj);
   return count;
}

inline void
wref_add(Eo *obj, Eo **wref)
{
   eo_wref_add(obj, wref);
}

inline void
wref_del(Eo *obj, Eo **wref)
{
   eo_wref_del(obj, wref);
}

} } }


#endif // EFL_CXX_DETAIL_EO_OPS_HH
