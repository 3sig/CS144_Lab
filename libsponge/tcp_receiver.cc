#include "tcp_receiver.hh"
#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    switch(receiver_state(*this)){
        case ERROR:
            break;
        case LISTEN:
        {
            if(!seg.header().syn){
                cerr << "Receive non-syn segment in LISTEN state" << endl;
                return;
            }
            _isn  = seg.header().seqno ;
            //_isn.emplace( wrap(0,seg.header().seqno));
            _checkpoint = 0;
            //state change to SYN_RECV
            if(receiver_state(*this)==SYN_RECV) segment_received(seg);

        }
            break;
        case SYN_RECV:
        {
            if(seg.header().seqno==_isn.value() && !seg.header().syn && _checkpoint < (1l<<32)){
                break;
            }
            uint64_t abs_seq =unwrap( seg.header().seqno ,_isn.value(),_checkpoint);
            uint64_t stream_index = abs_seq ? abs_seq-1 : 0;
            _reassembler.push_substring(seg.payload().copy(),stream_index,seg.header().fin );
            //_checkpoint = _reassembler.stream_out().bytes_written() - _reassembler.unassembled_bytes();
            _checkpoint = _reassembler.stream_out().bytes_read()+ _reassembler.stream_out().buffer_size();
        }
            break;
        case FIN_RECV:
            break;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn.has_value()){
        return std::nullopt;
    };
    uint64_t dx = (stream_out().input_ended()) ? 2 : 1;
    return std::optional<WrappingInt32>{  wrap(_checkpoint+dx, _isn.value()) };
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}

TCPReceiverState receiver_state(const TCPReceiver& receiver){
    if(receiver.stream_out().error()){
        return ERROR;
    }else if(not receiver.ackno().has_value()){
        return LISTEN;
    }else if(receiver.stream_out().input_ended()){
        return FIN_RECV;
    }else {
        return SYN_RECV;
    }
}

