extern void check_overflow(char*, int sz);

#define SVF_MEMCPY(dst, src, sz) \
    check_overflow(dst, sz); \
    check_overflow(src, sz); \
    int i; \
    for (i = 0; i < sz; i++) { \
        dst[i] = src[i]; \
    }

#define SVF_MEMSET(dst, elem, sz) \
    check_overflow(dst, sz); \
    int i; \
    for (i = 0; i < sz; i++) { \
        dst[i] = elem; \
    }


//llvm.memcpy.p0i8.p0i8.i64
void svf_llvm_memcpy_p0i8_p0i8_i64(char* dst, char* src, int sz) {
    SVF_MEMCPY(dst, src, sz);
}

void svf_llvm_memmove_p0i8_p0i8_i64(char* dst, char* src, int sz) {
    SVF_MEMCPY(dst, src, sz);
}

void svf___memcpy_chk(char* dst, char* src, int sz) {
    SVF_MEMCPY(dst, src, sz);
}

void svf___memmove_chk(char* dst, char* src, int sz) {
    SVF_MEMCPY(dst, src, sz);
}

void svf_llvm_memset(char* dst, char elem, int sz) {
    SVF_MEMSET(dst, elem, sz);
}

void svf_llvm_memset_p0i8_i32(char* dst, char elem, int sz) {
    SVF_MEMSET(dst, elem, sz);
}

void svf_llvm_memset_p0i8_i64(char* dst, char elem, int sz) {
    SVF_MEMSET(dst, elem, sz);
}

void svf___memset_chk(char* dst, char elem, int sz) {
    SVF_MEMSET(dst, elem, sz);
}

void* svf___dynamic_cast(void* source, const void* sourceTypeInfo, const void* targetTypeInfo, unsigned long castType)
{
    return source;
}
