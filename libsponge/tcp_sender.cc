#include "tcp_sender.hh"
#include <iostream>

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
	, _rto{_initial_retransmission_timeout}, _cur_rto(-1)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ack_seqno; }

void TCPSender::fill_window() {
	uint64_t window = _right_bound - _next_seqno;
	if(_right_bound < _next_seqno) return;
	if(_right_bound == _next_seqno && _ack_seqno == _next_seqno) window = 1;
	string s;
	TCPSegment seg;
	while(window > 0 && !_fin_sent) {
		seg.header().seqno = wrap(_next_seqno, _isn);
		seg.header().syn = _next_seqno == 0;
		seg.header().fin = _stream.eof();
		if(_next_seqno == 0 && _stream.eof())
			throw std::runtime_error("SYN and FIN at the same time!");
		s = _stream.read(min(TCPConfig::MAX_PAYLOAD_SIZE, window) - (_next_seqno == 0) - (_stream.eof()));
		seg.payload() = Buffer(std::move(s));
		if(_stream.eof() && seg.length_in_sequence_space() < window)
			seg.header().fin = true; // Piggyback FIN
		if(seg.length_in_sequence_space() == 0) break;
		if(_outstanding.empty())
			_cur_rto = _rto;
		_outstanding.push(seg);
		_segments_out.push(seg);
		if(seg.length_in_sequence_space() > window) throw std::runtime_error("TCP segment larger than window!");
		window -= seg.length_in_sequence_space();
		_next_seqno += seg.length_in_sequence_space();
		_fin_sent |= seg.header().fin;
	}
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
	uint64_t ack_new = unwrap(ackno, _isn, _ack_seqno);
	while(!_outstanding.empty()) {
		const auto& f = _outstanding.front();
		if(unwrap(f.header().seqno, _isn, _ack_seqno) + f.length_in_sequence_space() <= ack_new) {
			_outstanding.pop();
			_rto = _initial_retransmission_timeout;
			_cur_rto = _outstanding.empty() ? -1 : _rto;
			_fails = 0;
		} else break;
	}
	_ack_seqno = max(ack_new, _ack_seqno);
	if(ack_new + window_size > _right_bound) {
		_right_bound = ack_new + window_size;
	}
	fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
	if(_cur_rto <= ms_since_last_tick) {
		if(_outstanding.empty()) throw std::runtime_error("RTO expired with no outstanding TCP segment!");
		_segments_out.push(_outstanding.front());
		if(_right_bound > _ack_seqno) {
			++_fails;
			_rto *= 2;
		}
		_cur_rto = _rto;
	} else _cur_rto -= ms_since_last_tick;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _fails; }

void TCPSender::send_empty_segment() {
	TCPSegment seg;
	seg.header().seqno = wrap(_next_seqno, _isn);
	_segments_out.push(seg);
}
