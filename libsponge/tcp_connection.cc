#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _current_time - _last_recv_time;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    if(seg.header().rst){
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    _receiver.segment_received(seg);
    _last_recv_time = _current_time;
    if(seg.header().ack){
        _sender.ack_received(seg.header().ackno,seg.header().win);
    }
    if(seg.length_in_sequence_space()){
        if(_sender.segments_out().empty()){
            _sender.fill_window();
        }
        if(_sender.segments_out().empty()){
            _sender.send_empty_segment();
        }
    }

    if(_receiver.ackno()
        && (seg.length_in_sequence_space() == 0)
        && seg.header().seqno == _receiver.ackno().value()-1)
    {
        _sender.send_empty_segment();
    }
    do_send();

    if(_receiver.stream_out().input_ended()
        && ((not _sender.stream_in().eof())
            || (_sender.next_seqno_absolute() < _sender.stream_in().bytes_written()+2))){
        _linger_after_streams_finish = false;
    }


}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    _sender.fill_window();
    do_send();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    _current_time += ms_since_last_tick;

    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
        send_rst();
        return;
    }
    do_send();

    //CLEANLY END
    //   Prereq #1
    if(not (inbound_stream().eof() && inbound_stream().buffer_empty())){
        return;
    }
    //  Prereq #2
    if(not (_sender.stream_in().eof()
            && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written()+2)){
        return;
    }
    //  Prereq #3
    if(not (_sender.bytes_in_flight() == 0)){
        return;
    }

    if(not _linger_after_streams_finish){
        _active = false;
    }else if(_current_time >= _last_recv_time + 10* _cfg.rt_timeout){
        _active = false;
        _linger_after_streams_finish = false;
    }

}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    do_send();
}

void TCPConnection::connect() {
    _sender.fill_window();
    do_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


void TCPConnection::do_send(){
    while(!_sender.segments_out().empty()){
        TCPSegment& seg = _sender.segments_out().front();
        if(_receiver.ackno().has_value()){
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        uint64_t wsize = _receiver.window_size();
        seg.header().win = wsize < std::numeric_limits<uint16_t>::max() ? wsize: numeric_limits<uint16_t>::max();
        segments_out().push(std::move(seg));
        _sender.segments_out().pop();
    }
}

void TCPConnection::send_rst(){
    while(not _sender.segments_out().empty()){
        _sender.segments_out().pop();
    }
    _sender.send_empty_segment();
    TCPSegment& seg = _sender.segments_out().front();
    seg.header().rst = true;
    segments_out().push(std::move(seg));
    _sender.segments_out().pop();

    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    _linger_after_streams_finish = false;
}