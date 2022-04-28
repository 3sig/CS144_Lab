#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(_initial_retransmission_timeout)
{

}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ack_no; }

void TCPSender::fill_window() {
    if(sender_state(*this)==SERROR){
        return;
    }
    uint64_t win = _window_size ? _window_size:1;
    uint64_t window_remained = win > bytes_in_flight() ? win-bytes_in_flight() : 0;
    while(window_remained &&
           (!stream_in().buffer_empty()  // has data to be sent
            || sender_state(*this) == CLOSED  // no syn sent
            || sender_state(*this) == SYN_ACKED_2  // stream_in.eof() && no fin sent
            )){

        TCPSegment segment;
        segment.header().seqno = next_seqno();
        size_t payload_limit = TCPConfig::MAX_PAYLOAD_SIZE;
        // set SYN
        if(sender_state(*this) == CLOSED){
            segment.header().syn = true;
        }

        string payload_str = stream_in().read(std::min(payload_limit,window_remained-segment.length_in_sequence_space()));
        segment.payload() = Buffer(std::move(payload_str));
        // set FIN if state is SYN_ACK2 and segment still has payload space
        if(sender_state(*this) == SYN_ACKED_2){
            if(segment.length_in_sequence_space() < window_remained ){
                segment.header().fin = true;
            }
        }

        _next_seqno += segment.length_in_sequence_space();

        if(not _timer.running()){
            _timer.start();
            _timer.set_basetime(_current_time);
        }
        window_remained -= segment.length_in_sequence_space();

        _segments_without_ack.push_back(segment);
        segments_out().push(std::move(segment));

    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {

    uint64_t ackno_received = unwrap(ackno,_isn,_next_seqno);
    uint64_t window_received = window_size;

    // stale || invalid ack
    if(ackno_received < _ack_no || ackno_received > _next_seqno){
        return ;
    }
    _window_size = window_received;
    if(ackno_received==_ack_no)return;

    // received new ack
    _ack_no = ackno_received;
    _timer.rto() = _initial_retransmission_timeout;
    _timer.set_basetime(_current_time);

    if(bytes_in_flight())
        _timer.start();
    else
        _timer.stop(); // todo:  check code correctness

    _consecutive_retransmissions = 0;

    while(not _segments_without_ack.empty()){
        TCPSegment& seg = _segments_without_ack.front();
        uint64_t seg_seq = unwrap(seg.header().seqno,_isn,_next_seqno);
        uint64_t seg_ack = seg_seq + seg.length_in_sequence_space();
        if(seg_ack > _ack_no) break;
        _segments_without_ack.pop_front();
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _current_time += ms_since_last_tick;

    if(_timer.timeout(_current_time)){

        segments_out().push(_segments_without_ack.front());

        if(_window_size>0){
            ++_consecutive_retransmissions;
            _timer.rto() *= 2;
        }

        _timer.set_basetime(_current_time);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_retransmissions;
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    segments_out().push(std::move(segment));
}


TCPSenderState sender_state(TCPSender& sender){
    if(sender.stream_in().error()){
        return SERROR;
    }else if(sender.next_seqno_absolute() == 0){
        return CLOSED;
    }else if(sender.next_seqno_absolute()==sender.bytes_in_flight() ){
        return SYN_SENT;
    }else if(not sender.stream_in().eof()){
        return SYN_ACKED;
    }else if(sender.next_seqno_absolute() < sender.stream_in().bytes_written()+2){
        // SYN &FIN each occupied one sequence
        return SYN_ACKED_2;
    }else if(sender.bytes_in_flight() > 0){
        return FIN_SENT;
    }else {
        return FIN_ACKED;
    }
}
