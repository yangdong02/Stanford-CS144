#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

const uint64_t INFULL = -1;
template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
	const TCPHeader &hdr = seg.header();
	if(hdr.syn) _isn = hdr.seqno, _last = 0;
	if(_last == INFULL) return;
	uint64_t idx = unwrap(hdr.seqno+hdr.syn, _isn, _last) - 1;
	_reassembler.push_substring(seg.payload().copy(), idx, hdr.fin);
	_last = _reassembler.last_unreassembled(); // reassembler's last unreassembled == last reassembled absolute seqno
	if(hdr.fin) _fin = idx + seg.payload().size();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
	if(_last == INFULL) return {};
	return wrap(_last+1+(_last==_fin), _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.window_size(); }
