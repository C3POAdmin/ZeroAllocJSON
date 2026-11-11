#include "ZeroAllocJSON.h"
#include <assert.h>
#include <stdlib.h>
#include <new>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
static size_t allocations=0;
void* operator new(size_t n) {++allocations;void* p=malloc(n);if(!p) abort();return p;}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {free(p);}
void operator delete[](void* p) noexcept {free(p);}
using S=ZeroAllocJSON::Status;
template<size_t N> ZeroAllocJSON json(const char (&s)[N]) {return ZeroAllocJSON(s,N-1);}
template<class T> void integer(const char* maximum,const char* minimum,const char* overflow,const char* underflow) {
    auto hi=ZeroAllocJSON(maximum,strlen(maximum)).get<T>("x");
    assert(hi && hi.value==(std::numeric_limits<T>::max)());
    auto lo=ZeroAllocJSON(minimum,strlen(minimum)).get<T>("x");
    assert(lo && lo.value==(std::numeric_limits<T>::min)());
    assert(ZeroAllocJSON(overflow,strlen(overflow)).get<T>("x").status==S::OutOfRange);
    assert(ZeroAllocJSON(underflow,strlen(underflow)).get<T>("x").status==S::OutOfRange);
}
struct Sink {char data[256];size_t n; size_t write(const uint8_t* p,size_t len) {assert(n+len<=sizeof data);memcpy(data+n,p,len);n+=len;return len;}};
int main() {
    const size_t before=allocations;
    integer<uint8_t>("{\"x\":255}","{\"x\":0}","{\"x\":256}","{\"x\":-1}");
    integer<uint16_t>("{\"x\":65535}","{\"x\":0}","{\"x\":65536}","{\"x\":-1}");
    integer<uint32_t>("{\"x\":4294967295}","{\"x\":0}","{\"x\":4294967296}","{\"x\":-1}");
    integer<uint64_t>("{\"x\":18446744073709551615}","{\"x\":0}","{\"x\":18446744073709551616}","{\"x\":-1}");
    integer<int8_t>("{\"x\":127}","{\"x\":-128}","{\"x\":128}","{\"x\":-129}");
    integer<int16_t>("{\"x\":32767}","{\"x\":-32768}","{\"x\":32768}","{\"x\":-32769}");
    integer<int32_t>("{\"x\":2147483647}","{\"x\":-2147483648}","{\"x\":2147483648}","{\"x\":-2147483649}");
    integer<int64_t>("{\"x\":9223372036854775807}","{\"x\":-9223372036854775808}","{\"x\":9223372036854775808}","{\"x\":-9223372036854775809}");
    const char source[]=R"({"name":"Front Gate","empty":"","quoted":"a\"b\\c,}:x","enabled":true,"off":false,"n":null,"threshold":-12.75e+2})";
    ZeroAllocJSON cfg(source,sizeof(source)-1);
    assert(cfg.validate()==S::Ok);
    auto name=cfg.getString("name");assert(name && name.value.ptr==source+9 && name.value.length==10);
    assert(cfg.getString("empty").value.length==0);
    assert(cfg.getString("quoted").value.length==11);
    assert(cfg.get<bool>("enabled").value && !cfg.get<bool>("off").value);
    assert(cfg.isNull("n").value && !cfg.isNull("name").value);
    assert(cfg.get<int>("n").status==S::NullValue);
    assert(cfg.getString("n").status==S::NullValue);
    assert(cfg.get<int>("missing").status==S::MissingKey);
    assert(cfg.get<int>(0).status==S::InvalidArgument);
    assert(cfg.get<int>("name").status==S::TypeMismatch);
    assert(cfg.get<bool>("threshold").status==S::TypeMismatch);
    assert(cfg.get<int>("threshold").status==S::TypeMismatch);
    assert(cfg.get<float>("threshold").value==-1275);
    assert(cfg.get<double>("threshold").value==-1275);
    assert(json("{\"x\":1e99999}").get<double>("x").status==S::OutOfRange);
    assert(json("{\"x\":1e-99999}").get<double>("x").status==S::OutOfRange);
    assert(json("{\"x\":1e39}").get<float>("x").status==S::OutOfRange);
    assert(json("{\"x\":1e-50}").get<float>("x").status==S::OutOfRange);
    assert(json("{\"x\":1e-40}").get<float>("x"));
    assert(json("{\"x\":4.9406564584124654e-324}").get<double>("x"));
    assert(json("{\"x\":0e99999}").get<double>("x").value==0);
    assert(json("{\"x\":0.000000000000000000000123}").get<double>("x").value>1.22e-22);
    assert(json("{\"x\":1234567890123456789012345}").get<double>("x").value>1.23e24);
    assert(json(" \r\n { \t\"x\" : 7 } \n").get<int>("x").value==7);
    assert(json("{\"\\u0078\":1}").get<int>("x").value==1);
    assert(json("{\"x\":1,\"\\u0078\":2}").validate()==S::DuplicateKey);
    assert(json("{\"é\":1,\"\\u00e9\":2}").validate()==S::DuplicateKey);
    assert(json("{\"\\ud83d\\ude00\":3}").get<int>("😀").value==3);
    assert(json("{\"x\":{},\"y\":1}").validate()==S::Unsupported);
    assert(json("{\"x\":[]}").validate()==S::Unsupported);
    const char* invalid[]={"", "{", "[]", "null", "{\"x\":01}","{\"x\":+1}","{\"x\":.1}","{\"x\":1.}","{\"x\":1e}","{\"x\":tru}","{\"x\":truex}","{\"x\":NaN}","{\"x\":1,}","{\"x\" 1}","{\"x\":1}x","{\"x\":\"\\q\"}","{\"x\":\"\\uD800\"}","{\"x\":\"\\uDC00\"}","{\"x\":\"\\uXX00\"}","{\"x\":\"\n\"}","{\"x\":\"\xc0\x80\"}","{\"x\":\"\xed\xa0\x80\"}"};
    for(auto s:invalid) {assert(ZeroAllocJSON(s,strlen(s)).validate()==S::InvalidJSON);assert(!ZeroAllocJSON(s,strlen(s)).get<int>("x"));}
    assert(ZeroAllocJSON(0,0).validate()==S::InvalidArgument);
    assert(json("{}").validate()==S::Ok);
    Sink out={{},0};assert(cfg.showVariable("enabled",out)==S::Ok);assert(out.n==13 && memcmp(out.data,"enabled: true",13)==0);
    out.n=0;assert(cfg.showJSON(out)==S::Ok && out.n==sizeof(source)-1);
    for(int i=0;i<1000;++i) assert(cfg.get<bool>("enabled").value);
    // Every prefix placed against an inaccessible page: no terminator and no readable padding.
    size_t page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    char* mem=static_cast<char*>(mmap(0,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    assert(mem!=MAP_FAILED);assert(mprotect(mem+page,page,PROT_NONE)==0);
    for(size_t n=0;n<=sizeof(source)-1;++n) {
        char* start=mem+page-n;memcpy(start,source,n);
        ZeroAllocJSON bounded(start,n);
        assert(bounded.validate()==(n==sizeof(source)-1 ? S::Ok : S::InvalidJSON));
        bounded.get<int>("threshold");bounded.getString("name");
    }
    // Deterministic arbitrary bytes exercise scanner branches with ASan/UBSan.
    uint32_t state=12345;char fuzz[128];
    for(int trial=0;trial<20000;++trial) {
        size_t n=static_cast<size_t>(trial)%sizeof(fuzz);
        for(size_t i=0;i<n;++i) {state=state*1664525+1013904223;fuzz[i]=static_cast<char>(state>>24);}
        char* edge=mem+page-n;memcpy(edge,fuzz,n);
        ZeroAllocJSON f(edge,n); f.validate();f.get<double>("x");f.getString("x");
    }
    // Mutate valid documents to reach deeper scanner states at a protected boundary.
    const char* corpus[]={source,R"({"\ud83d\ude00":"\u0078","x":1e-20})",R"({"é":"😀","x":-0.12E+30})"};
    for(auto seed:corpus) {
        size_t length=strlen(seed);
        for(size_t i=0;i<length;++i) {
            for(unsigned byte=0;byte<256;++byte) {
                char* edge=mem+page-length;memcpy(edge,seed,length);edge[i]=static_cast<char>(byte);
                ZeroAllocJSON mutated(edge,length);mutated.validate();mutated.get<double>("x");mutated.getString("name");
            }
        }
    }
    munmap(mem,page*2);
    assert(allocations==before);
    puts("All tests passed; zero C++ heap allocations during tests.");
}
