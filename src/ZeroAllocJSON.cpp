#include "ZeroAllocJSON.h"
namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}
void ZeroAllocJSON::space(size_t& p) const {
    while (p < size_ && (data_[p]==' ' || data_[p]=='\t' || data_[p]=='\r' || data_[p]=='\n')) ++p;
}
// Called only on previously validated string contents; returns a Unicode scalar.
uint32_t ZeroAllocJSON::character(size_t& p) const {
    uint32_t c = static_cast<unsigned char>(data_[p++]);
    if (c == '\\') {
        char e = data_[p++];
        switch (e) {
        case 'b': return 8; case 'f': return 12; case 'n': return 10;
        case 'r': return 13; case 't': return 9;
        case 'u': {
            c = 0; for (int i=0;i<4;++i) c = c*16 + hex(data_[p++]);
            if (c >= 0xD800 && c <= 0xDBFF) {
                p += 2; uint32_t lo = 0;
                for (int i=0;i<4;++i) lo = lo*16 + hex(data_[p++]);
                c = 0x10000 + ((c-0xD800)<<10) + lo-0xDC00;
            }
            return c;
        }
        default: return static_cast<unsigned char>(e);
        }
    }
    if (c < 0x80) return c;
    int remaining = c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
    c &= remaining == 1 ? 0x1F : remaining == 2 ? 0x0F : 7;
    while (remaining--) c = (c<<6) | (static_cast<unsigned char>(data_[p++]) & 63);
    return c;
}
ZeroAllocJSON::Status ZeroAllocJSON::string(size_t& p, Token& t) const {
    t.begin = p; t.kind = Kind::String;
    if (p == size_ || data_[p++] != '"') return Status::InvalidJSON;
    while (p < size_) {
        unsigned char c = data_[p++];
        if (c == '"') { t.end = p; return Status::Ok; }
        if (c < 0x20) return Status::InvalidJSON;
        if (c == '\\') {
            if (p == size_) return Status::InvalidJSON;
            char e = data_[p++];
            if (e == 'u') {
                uint32_t u = 0;
                for (int i=0;i<4;++i) {
                    if (p == size_ || hex(data_[p]) < 0) return Status::InvalidJSON;
                    u = u*16 + hex(data_[p++]);
                }
                if (u >= 0xDC00 && u <= 0xDFFF) return Status::InvalidJSON;
                if (u >= 0xD800 && u <= 0xDBFF) {
                    if (size_-p < 6 || data_[p]!='\\' || data_[p+1]!='u') return Status::InvalidJSON;
                    p += 2; uint32_t lo = 0;
                    for (int i=0;i<4;++i) {
                        if (hex(data_[p]) < 0) return Status::InvalidJSON;
                        lo = lo*16 + hex(data_[p++]);
                    }
                    if (lo < 0xDC00 || lo > 0xDFFF) return Status::InvalidJSON;
                }
            } else if (e!='"' && e!='\\' && e!='/' && e!='b' && e!='f' && e!='n' && e!='r' && e!='t') return Status::InvalidJSON;
        } else if (c >= 0x80) {
            int n; uint32_t u; uint32_t minimum;
            if (c>=0xC2 && c<=0xDF) {n=1;u=c&31;minimum=0x80;}
            else if(c>=0xE0 && c<=0xEF) {n=2;u=c&15;minimum=0x800;}
            else if(c>=0xF0 && c<=0xF4) {n=3;u=c&7;minimum=0x10000;}
            else return Status::InvalidJSON;
            while(n--) {
                if(p==size_) return Status::InvalidJSON;
                unsigned char b=data_[p++];
                if((b&0xC0)!=0x80) return Status::InvalidJSON;
                u=(u<<6)|(b&63);
            }
            if(u<minimum || u>0x10FFFF || (u>=0xD800 && u<=0xDFFF)) return Status::InvalidJSON;
        }
    }
    return Status::InvalidJSON;
}
ZeroAllocJSON::Status ZeroAllocJSON::value(size_t& p, Token& t) const {
    if (p==size_) return Status::InvalidJSON;
    t.begin=p;
    if(data_[p]=='"') return string(p,t);
    if(data_[p]=='{' || data_[p]=='[') return Status::Unsupported;
    const char* word = 0; size_t n=0;
    if(data_[p]=='t') {word="true";n=4;t.kind=Kind::Boolean;}
    if(data_[p]=='f') {word="false";n=5;t.kind=Kind::Boolean;}
    if(data_[p]=='n') {word="null";n=4;t.kind=Kind::Null;}
    if(word) {
        if(size_-p<n) return Status::InvalidJSON;
        for(size_t i=0;i<n;++i) if(data_[p+i]!=word[i]) return Status::InvalidJSON;
        p+=n;
    } else {
        t.kind=Kind::Number;
        if(data_[p]=='-') ++p;
        if(p==size_ || !digit(data_[p])) return Status::InvalidJSON;
        if(data_[p]=='0') ++p; else while(p<size_ && digit(data_[p])) ++p;
        if(p<size_ && data_[p]=='.') {
            ++p; size_t start=p; while(p<size_ && digit(data_[p])) ++p;
            if(start==p) return Status::InvalidJSON;
        }
        if(p<size_ && (data_[p]=='e' || data_[p]=='E')) {
            ++p; if(p<size_ && (data_[p]=='+' || data_[p]=='-')) ++p;
            size_t start=p; while(p<size_ && digit(data_[p])) ++p;
            if(start==p) return Status::InvalidJSON;
        }
    }
    t.end=p; return Status::Ok;
}
bool ZeroAllocJSON::equal(const Token& a, const Token& b) const {
    size_t p=a.begin+1,q=b.begin+1;
    while(p<a.end-1 && q<b.end-1) if(character(p)!=character(q)) return false;
    return p==a.end-1 && q==b.end-1;
}
bool ZeroAllocJSON::equal(const Token& a, const char* key) const {
    // Compare decoded key to UTF-8 supplied by caller, without a temporary buffer.
    size_t p=a.begin+1,q=0;
    while(p<a.end-1) {
        uint32_t c=character(p); unsigned char bytes[4]; size_t n=0;
        if(c<0x80) bytes[n++]=static_cast<unsigned char>(c);
        else if(c<0x800) {bytes[n++]=static_cast<unsigned char>(0xC0|(c>>6));bytes[n++]=0x80|(c&63);}
        else if(c<0x10000) {bytes[n++]=static_cast<unsigned char>(0xE0|(c>>12));bytes[n++]=0x80|((c>>6)&63);bytes[n++]=0x80|(c&63);}
        else {bytes[n++]=static_cast<unsigned char>(0xF0|(c>>18));bytes[n++]=0x80|((c>>12)&63);bytes[n++]=0x80|((c>>6)&63);bytes[n++]=0x80|(c&63);}
        for(size_t i=0;i<n;++i) if(!key[q] || static_cast<unsigned char>(key[q++])!=bytes[i]) return false;
    }
    return key[q]==0;
}
ZeroAllocJSON::Status ZeroAllocJSON::scan(const char* key, Token* result) const {
    if(!data_) return Status::InvalidArgument;
    size_t p=0; space(p);
    if(p==size_ || data_[p++]!='{') return Status::InvalidJSON;
    const size_t first=p; bool found=false; space(p);
    if(p<size_ && data_[p]=='}') ++p;
    else for(;;) {
        Token k={},v={}; Status s=string(p,k); if(s!=Status::Ok) return s;
        // Re-scan earlier members to detect duplicates without per-key storage.
        size_t q=first; space(q);
        while(q<k.begin) {
            Token previous={}, ignored={}; s=string(q,previous); if(s!=Status::Ok) return s;
            if(equal(k,previous)) return Status::DuplicateKey;
            space(q); ++q; space(q); s=value(q,ignored); if(s!=Status::Ok) return s;
            space(q); ++q; space(q);
        }
        space(p); if(p==size_ || data_[p++]!=':') return Status::InvalidJSON;
        space(p); s=value(p,v); if(s!=Status::Ok) return s;
        if(key && equal(k,key)) {found=true; *result=v;}
        space(p); if(p==size_) return Status::InvalidJSON;
        char delimiter=data_[p++]; if(delimiter=='}') break;
        if(delimiter!=',') return Status::InvalidJSON;
        space(p);
    }
    space(p); if(p!=size_) return Status::InvalidJSON;
    return key && !found ? Status::MissingKey : Status::Ok;
}
ZeroAllocJSON::Status ZeroAllocJSON::validate() const { return scan(0,0); }
ZeroAllocJSON::Status ZeroAllocJSON::find(const char* key, Token& t) const {
    return key ? scan(key,&t) : Status::InvalidArgument;
}
ZeroAllocJSON::Result<ZeroAllocJSON::StringView> ZeroAllocJSON::getString(const char* key) const {
    Token t={}; Status s=find(key,t);
    if(s==Status::Ok && t.kind!=Kind::String) s=t.kind==Kind::Null ? Status::NullValue : Status::TypeMismatch;
    return Result<StringView>{s==Status::Ok ? StringView{data_+t.begin+1,t.end-t.begin-2} : StringView{0,0},s};
}
ZeroAllocJSON::Result<bool> ZeroAllocJSON::isNull(const char* key) const {
    Token t={}; Status s=find(key,t); return Result<bool>{s==Status::Ok && t.kind==Kind::Null,s};
}
ZeroAllocJSON::Status ZeroAllocJSON::magnitude(const Token& t,uint64_t& v,bool& neg) const {
    if(t.kind!=Kind::Number) return Status::TypeMismatch;
    size_t p=t.begin; neg=data_[p]=='-'; if(neg) ++p;
    // Reject fractions/exponents even when mathematically integral.
    for(size_t i=p;i<t.end;++i) if(!digit(data_[i])) return Status::TypeMismatch;
    for(;p<t.end;++p) {
        unsigned d=data_[p]-'0';
        if(v>(UINT64_MAX-d)/10) return Status::OutOfRange;
        v=v*10+d;
    }
    return Status::Ok;
}
ZeroAllocJSON::Status ZeroAllocJSON::floating(const Token& t,long double& v) const {
    if(t.kind!=Kind::Number) return Status::TypeMismatch;
    size_t p=t.begin; bool neg=data_[p]=='-'; if(neg) ++p;
    // Retain 19 significant digits; account for omitted digits in the exponent.
    int kept=0; long double scale=0; bool fraction=false, significant=false;
    for(;p<t.end && data_[p]!='e' && data_[p]!='E';++p) {
        if(data_[p]=='.') {fraction=true;continue;}
        int d=data_[p]-'0'; if(fraction) scale-=1;
        if(d || significant) {
            significant=true;
            if(kept<19) {v=v*10+d;++kept;} else scale+=1;
        }
    }
    long double exponent=0; bool expNeg=false;
    if(p<t.end) {
        ++p; if(data_[p]=='-' || data_[p]=='+') {expNeg=data_[p]=='-';++p;}
        for(;p<t.end;++p) {if(exponent<1e20L) exponent=exponent*10+(data_[p]-'0');}
    }
    if(v==0) {v=neg ? -0.0L : 0.0L;return Status::Ok;}
    scale+=expNeg ? -exponent : exponent;
    // Chunk scaling avoids premature underflow of the power itself.
    if(scale>std::numeric_limits<long double>::max_exponent10+20L ||
       scale<std::numeric_limits<long double>::min_exponent10-100L) return Status::OutOfRange;
    while(scale>0) {long double step=scale>100 ? 100 : scale;v*=powl(10,step);scale-=step;}
    while(scale<0) {long double step=scale< -100 ? -100 : scale;v*=powl(10,step);scale-=step;}
    if(!isfinite(v) || v==0) return Status::OutOfRange;
    if(neg) v=-v;
    return Status::Ok;
}
const char* ZeroAllocJSON::statusName(Status s) {
    switch(s) {
    case Status::Ok:return "Ok";case Status::InvalidJSON:return "InvalidJSON";
    case Status::Unsupported:return "Unsupported";case Status::DuplicateKey:return "DuplicateKey";
    case Status::MissingKey:return "MissingKey";case Status::NullValue:return "NullValue";
    case Status::TypeMismatch:return "TypeMismatch";case Status::OutOfRange:return "OutOfRange";
    case Status::InvalidArgument:return "InvalidArgument";
    } return "Unknown";
}
