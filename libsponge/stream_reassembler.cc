#include "stream_reassembler.hh"
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity),offset(0),unassembled_count(0),buf_status(capacity),buffer(capacity,false),_eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(_output.eof())return;

    //always store stream[index] in buffer[i%capacity]
    size_t outbuf_size = _output.buffer_size();
    size_t i;
    for(i = 0;i<data.size();++i){
        size_t pos = index + i;
        if(pos>=offset + _capacity - outbuf_size )break;
        size_t real_pos = pos%_capacity;

        if(buf_status[real_pos])continue;

        buf_status[real_pos] = true;
        ++unassembled_count;
        buffer[real_pos] = data[i];
    }
    if(eof && i==data.size())_eof = true;
    // bytes to be assembled
    size_t unassembled_begin = offset;
    unassembled_begin = unassembled_begin % _capacity;
    string tmp;
    while(buf_status[unassembled_begin]){
        buf_status[unassembled_begin] = false;
        tmp.push_back(buffer[unassembled_begin]);
        ++unassembled_begin;
        unassembled_begin %= _capacity;
        --unassembled_count;
    }
    offset += tmp.size();
    _output.write(tmp);
    if(eof && unassembled_count==0)_output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_count; }

bool StreamReassembler::empty() const { return _output.buffer_empty() && unassembled_count==0; }
