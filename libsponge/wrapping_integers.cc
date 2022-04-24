#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
static const uint64_t modulo = (1ul << 32);
//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    n = n+(isn.raw_value());
    n = n%modulo;
    return WrappingInt32{static_cast<uint32_t>(n)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t seq = n.raw_value(), isn_t = isn.raw_value();
    if(seq < isn_t)seq += modulo;
    seq = seq - isn_t;
    if(seq >= checkpoint)return seq;
    uint64_t tail = checkpoint % modulo;
    if(seq > tail && seq - tail > (1ul << 31)){
        seq = seq + (checkpoint-tail) - (1ul <<32);
    }else if(seq < tail && tail-seq >(1ul <<31)){
        seq = seq + (checkpoint-tail) + modulo;
    }else{
        seq = seq + (checkpoint-tail);
    }
    return seq;
}
