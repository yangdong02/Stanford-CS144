#include "stream_reassembler.hh"
#include <cassert>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
	_output(capacity), _capacity(capacity), _ind(0), _eofidx(-1), _ucount(0), _unr(capacity), _vis(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
	if(eof) _eofidx = index + data.length();
	size_t ci = index;
	for(auto ch : data) {
		if(ci < _ind) { ++ci; continue; }
		if(ci >= _output.bytes_read() + _capacity) break;
		if(!_vis[ci - _ind]) _vis[ci - _ind] = true, _ucount++;
		else assert(_unr[ci - _ind] == ch);
		_unr[ci - _ind] = ch, ++ci;
	}
	string s;
	while(!_unr.empty() && _vis[0]) {
		s += _unr[0];
		_unr.pop_front(), _vis.pop_front(), _ucount--, _ind++;
		_unr.push_back(0), _vis.push_back(0);
	}
	assert(_output.write(s) == s.length());
	if(_ind == _eofidx) _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _ucount; }

bool StreamReassembler::empty() const { return _ucount == 0; }
