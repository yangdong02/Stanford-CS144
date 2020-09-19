#include "byte_stream.hh"
#include <algorithm>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): cap(capacity) { }

size_t ByteStream::write(const string &data) {
	for(size_t i = 0; i < data.size(); ++i) {
		if(q.size() == buffer_size()) { bytes_written += i; return i; }
		q.push_back(data[i]);
	}
	_bytes_written += data.length();
    return data.length();
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
	size_t siz = std::min(len, q.size());
	_bytes_read += siz;
	return string(q.begin(), q.begin() + siz);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { for(int i = 0; i < len; ++i) q.pop_front(); }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
	string res;
	size_t siz = std::min(len, q.size());
	_bytes_read += siz;
	for(int i = 0; i < siz; ++i) {
		res += q.front();
		q.pop_front();
	}
	return res;
}

void ByteStream::end_input() { _ended = true; }

bool ByteStream::input_ended() const { return _ended; }

size_t ByteStream::buffer_size() const { return cap; }

bool ByteStream::buffer_empty() const { return q.empty(); }

bool ByteStream::eof() const { return _ended && q.empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return q.size() < cap; }
