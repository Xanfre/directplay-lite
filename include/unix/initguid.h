#ifdef DEFINE_GUID
#undef DEFINE_GUID
#endif

#ifdef __cplusplus
#define DEFINE_GUID(iid, a, b, c, d, e, f, g, h, i, j, k) \
  extern "C" const GUID iid = {a,b,c,{d,e,f,g,h,i,j,k}};
#else
#define DEFINE_GUID(iid, a, b, c, d, e, f, g, h, i, j, k) \
  const GUID iid = {a,b,c,{d,e,f,g,h,i,j,k}};
#endif // __cplusplus
