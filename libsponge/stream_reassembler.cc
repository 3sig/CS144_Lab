#include "stream_reassembler.hh"


// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(eof){
        _eof_position = index+data.size();
    }
    if(data.size() + index < stream_out().bytes_written())return ;
    // un-assembled window
    size_t _left_1 = std::max(index,stream_out().bytes_written());
    size_t _right_1 = std::min(index+data.size(),stream_out().bytes_written()+_capacity);

    // cut the non-repeated part of string, store it in map,
    size_t left = _left_1, right = left;
    while(left < _right_1){
        size_t next_left = left;
        auto it = _buffer.upper_bound(left);

        if(it == _buffer.end()){
            right = _right_1;
            next_left = right;
        }else{
            next_left = it->first;
            size_t buffer_right = it->first - it->second.size();
            right = std::min(buffer_right,_right_1);
        }
        if(right>left){
            size_t sub_size = right-left;
            _unassembled += sub_size;
            _buffer.insert({right,data.substr(left-index,sub_size)});
        }
        left = next_left;
    }

    assemble();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return _buffer.empty(); }

void StreamReassembler::assemble() {
    while(not _buffer.empty()){
        auto it = _buffer.begin();
        uint64_t index = it->first - it->second.size();
        if(index > stream_out().bytes_written()){
            break;
        }
        _unassembled -= it->second.size();

        stream_out().write(it->second);
        _buffer.erase(it);
    }
    if(stream_out().bytes_written() == _eof_position){
        stream_out().end_input();
    }
}