// Who knows where I got this from, I've completely forgot.
// is it not publicly available anywhere??

#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace gunzip_detail {

struct BitReader {
    const uint8_t* data;
    size_t size, byte_pos = 0;
    int bit_buf = 0, bit_count = 0;

    BitReader(const uint8_t* d, size_t s) : data(d), size(s) {}

    uint32_t bits(int n) {
        while (bit_count < n) {
            if (byte_pos >= size) throw std::runtime_error("gunzip: unexpected end of input");
            bit_buf |= data[byte_pos++] << bit_count;
            bit_count += 8;
        }
        uint32_t v = bit_buf & ((1 << n) - 1);
        bit_buf >>= n;
        bit_count -= n;
        return v;
    }

    uint8_t byte() {
        bit_buf = 0; bit_count = 0;
        if (byte_pos >= size) throw std::runtime_error("gunzip: unexpected end of input");
        return data[byte_pos++];
    }

    uint16_t u16() { uint16_t lo = byte(); return lo | (uint16_t(byte()) << 8); }
    size_t pos() const { return byte_pos; }
    void seek(size_t p) { byte_pos = p; bit_buf = 0; bit_count = 0; }
};

struct HuffTree {
    static constexpr int MAX_BITS = 15;
    uint16_t sym[1 << MAX_BITS] = {};

    void build(const uint8_t* lengths, int n) {
        int bl_count[MAX_BITS + 1] = {};
        for (int i = 0; i < n; i++) if (lengths[i]) bl_count[lengths[i]]++;

        int next_code[MAX_BITS + 1] = {};
        for (int bits = 1, code = 0; bits <= MAX_BITS; bits++)
            next_code[bits] = code = (code + bl_count[bits - 1]) << 1;

        for (int i = 0; i < n; i++) {
            int len = lengths[i];
            if (!len) continue;
            int code = next_code[len]++;
            int fill = MAX_BITS - len;
            for (int j = 0; j < (1 << fill); j++) {
                int idx = (j << len) | reverse_bits(code, len);
                sym[idx] = uint16_t((i << 4) | len);
            }
        }
    }

    int decode(BitReader& br) const {
        const uint32_t peek = br.bits(MAX_BITS);
        const uint16_t entry = sym[peek];
        const int len = entry & 0xf;
        if (!len) throw std::runtime_error("gunzip: bad huffman code");
        br.bit_buf = (br.bit_buf << len) | (peek >> len);
        return entry >> 4;
    }

    static int reverse_bits(int v, int n) {
        int r = 0;
        for (int i = 0; i < n; i++) { r = (r << 1) | (v & 1); v >>= 1; }
        return r;
    }
};

inline int huff_decode(BitReader& br, const HuffTree& tree) {
    while (br.bit_count < HuffTree::MAX_BITS) {
        if (br.byte_pos < br.size)
            br.bit_buf |= br.data[br.byte_pos++] << br.bit_count;
        br.bit_count += 8;
    }
    uint16_t entry = tree.sym[br.bit_buf & ((1 << HuffTree::MAX_BITS) - 1)];
    int len = entry & 0xf;
    if (!len) throw std::runtime_error("gunzip: bad huffman symbol");
    br.bit_buf >>= len;
    br.bit_count -= len;
    return entry >> 4;
}

static const uint16_t LENGTH_BASE[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t  LENGTH_EXTRA[29] = {0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0};
static const uint16_t DIST_BASE[30]    = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t  DIST_EXTRA[30]   = {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13};
static const uint8_t  CLCL_ORDER[19]   = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

inline void inflate_block(BitReader& br, std::vector<uint8_t>& out, bool fixed) {
    HuffTree ltree, dtree;

    if (fixed) {
        uint8_t ll[288], dl[32];
        for (int i=0;i<144;i++) ll[i]=8;
        for (int i=144;i<256;i++) ll[i]=9;
        for (int i=256;i<280;i++) ll[i]=7;
        for (int i=280;i<288;i++) ll[i]=8;
        for (int i=0;i<32;i++) dl[i]=5;
        ltree.build(ll, 288);
        dtree.build(dl, 32);
    } else {
        int hlit  = br.bits(5) + 257;
        int hdist = br.bits(5) + 1;
        int hclen = br.bits(4) + 4;

        uint8_t cl_lens[19] = {};
        for (int i = 0; i < hclen; i++) cl_lens[CLCL_ORDER[i]] = br.bits(3);
        HuffTree cltree; cltree.build(cl_lens, 19);

        uint8_t lens[288 + 32] = {};
        for (int i = 0; i < hlit + hdist; ) {
            int sym = huff_decode(br, cltree);
            if (sym < 16) { lens[i++] = sym; }
            else if (sym == 16) { int r=br.bits(2)+3; while(r--) lens[i]=lens[i-1], i++; }
            else if (sym == 17) { int r=br.bits(3)+3; i+=r; }
            else                { int r=br.bits(7)+11; i+=r; }
        }
        ltree.build(lens, hlit);
        dtree.build(lens + hlit, hdist);
    }

    for (;;) {
        int sym = huff_decode(br, ltree);
        if (sym < 256) {
            out.push_back(uint8_t(sym));
        } else if (sym == 256) {
            break;
        } else {
            int li = sym - 257;
            int length = LENGTH_BASE[li] + br.bits(LENGTH_EXTRA[li]);
            int di = huff_decode(br, dtree);
            int dist = DIST_BASE[di] + br.bits(DIST_EXTRA[di]);
            size_t start = out.size() - dist;
            for (int i = 0; i < length; i++) out.push_back(out[start + i]);
        }
    }
}

}

inline std::vector<uint8_t> gunzip(const std::vector<uint8_t>& in) {
    using namespace gunzip_detail;
    if (in.size() < 18) throw std::runtime_error("gunzip: input too small");

    if (in[0] != 0x1f || in[1] != 0x8b) throw std::runtime_error("gunzip: bad magic");
    if (in[2] != 8)                       throw std::runtime_error("gunzip: unsupported compression method");
    uint8_t flags = in[3];
    size_t pos = 10;
    if (flags & 0x04) { uint16_t xlen = in[pos] | (in[pos+1]<<8); pos += 2 + xlen; }
    if (flags & 0x08) { while (in[pos++] != 0); }
    if (flags & 0x10) { while (in[pos++] != 0); }
    if (flags & 0x02) { pos += 2; }

    BitReader br(in.data() + pos, in.size() - pos - 8);
    std::vector<uint8_t> out;
    out.reserve(in.size() * 4);

    bool final = false;
    while (!final) {
        final    = br.bits(1);
        int btype = br.bits(2);
        if (btype == 0) {
            br.bit_buf = 0; br.bit_count = 0;
            uint16_t len  = br.u16();
            uint16_t nlen = br.u16();
            if ((len ^ nlen) != 0xffff) throw std::runtime_error("gunzip: bad stored block length");
            for (int i = 0; i < len; i++) out.push_back(br.byte());
        } else if (btype == 1) {
            inflate_block(br, out, true);
        } else if (btype == 2) {
            inflate_block(br, out, false);
        } else {
            throw std::runtime_error("gunzip: reserved block type");
        }
    }

    return out;
}