#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buf(capacity + 1), beg(0), end(0) {}

size_t ByteStream::write(const string &data) {
    size_t i = 0;
    while (i < data.size()) {
        if ((end + 1) % buf.size() == beg) {
            break;
        }
        buf[end] = data[i++];
        end = (end + 1) % buf.size();
    }
    total_written += i;
    return i;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string str;
    str.reserve(len);
    size_t i = beg, read = 0;
    while (i != end && read < len) {
        str.push_back(buf[i]);
        i = (i + 1) % buf.size();
        ++read;
    }

    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t _len = std::min(len,buffer_size());
    beg += _len;
    total_read += _len;
    beg = beg % buf.size();
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string str = peek_output(len);
    pop_output(len);
    return str;
}

void ByteStream::end_input() { eof_flag = true; }

bool ByteStream::input_ended() const { return eof_flag; }

size_t ByteStream::buffer_size() const { return (end + buf.size() - beg) % buf.size(); }

bool ByteStream::buffer_empty() const { return beg == end; }

bool ByteStream::eof() const {
    if (!eof_flag)
        return false;
    return buffer_empty();
}

size_t ByteStream::bytes_written() const { return total_written; }

size_t ByteStream::bytes_read() const { return total_read; }

size_t ByteStream::remaining_capacity() const {
    int max_cap = buf.size() - 1;
    return max_cap - buffer_size();
}
