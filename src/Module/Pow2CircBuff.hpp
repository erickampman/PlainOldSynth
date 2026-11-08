//
//  Pow2CircBuff.h
//  Delay1Framework
//
//  Created by Eric Kampman on 1/14/19.
//

#ifndef Pow2CircBuff_h
#define Pow2CircBuff_h

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>


class Pow2CircBuff {
public:
    
    Pow2CircBuff(int bits = 4)
    : _bits(0), _mask(0), _buff(nullptr), _buff_size(0)
    {
        if (bits < Pow2CircBuff_MinBits || bits > Pow2CircBuff_MaxBits) {
            return;
        }

        _bits = bits;
        _buff_size = 1u << _bits;
        _mask = 0xffffffffu >> (32 - _bits);

        _buff = static_cast<float*>(std::calloc(_buff_size, sizeof(float)));
        if (!_buff) {
            _bits = 0;
            _mask = 0;
            _buff_size = 0;
        }
    }
    
    void clear() {
        if (_buff) {
            memset(_buff, 0, _buff_size * sizeof(float));
        }
    }
    
    ~Pow2CircBuff() {
        free(_buff);
    }
    
    void insert(float val) {
        *(_buff + _pos) = val;
        _pos = ((_pos+1) & _mask);
    }
    
    // The following two methods must be used as a pair.
    // allows us to treat pos 0 without conditionals
    void insertNoInc(float val) {
        *(_buff + _pos) = val;
    }
    // do this AFTER insertNoInc
    void incNoInsert() {
        _pos = ((_pos+1) & _mask);
    }
    
    // position in the past relative to now.
    // no range checking as this will happen while
    // processing audio
    float retrieve(size_t pos) {
        return *(_buff + ((_pos + _buff_size - pos) & _mask));
    }
    
    // Catmull-Rom cubic interpolation between adjacent integer-sample positions.
    // pos: fractional delay in samples, relative to the most-recently inserted value.
    // REQUIREMENT: pos >= 2.0.  When pos < 2 the (n-1) tangent sample aliases the
    // oldest (about-to-be-overwritten) buffer slot and produces incorrect results.
    float retrieveInterpolate(double pos) {
        size_t n    = (size_t)pos;
        float  t    = (float)(pos - (double)n);
        size_t base = _pos + _buff_size - n;

        // Four-point neighbourhood (past = decreasing buffer index).
        float y0 = *(_buff + ((base + 1) & _mask));   // (n-1) steps ago (more recent)
        float y1 = *(_buff + ( base      & _mask));   // n     steps ago
        float y2 = *(_buff + ((base - 1) & _mask));   // (n+1) steps ago
        float y3 = *(_buff + ((base - 2) & _mask));   // (n+2) steps ago

        // Catmull-Rom: result == y1 at t=0, y2 at t=1.
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
        const float a1 =       y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
        const float a2 = -0.5f*y0            + 0.5f*y2;
        return y1 + a2*t + a1*t2 + a0*t3;
    }
    
    size_t buffsize() {
        return _buff_size;
    }
    
    
private:
    int _bits;   // 2 <= bits <= 32
    uint32_t _mask;
    float *_buff;
    size_t _buff_size;
    size_t _pos = 0;
    const int Pow2CircBuff_MinBits  = 2;  // i.e. 4 slots
    const int Pow2CircBuff_MaxBits  = 32;
};


#endif /* Pow2CircBuff_h */
