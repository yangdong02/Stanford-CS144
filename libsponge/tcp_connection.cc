#include "tcp_connection.hh"

#include <iostream>
#include <cassert>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

void TCPConnection::segment_received(const TCPSegment &seg) {
	cerr << "[TCP debug] segment_received: " << seg.header().summary() << endl;
	cerr << "[TCPDEBUG] Incoming stream status: endinput? " << _sender.stream_in().input_ended() << " eof? "<<_sender.stream_in().eof() << endl;
	_time_since_last = 0;
	if(seg.header().rst) {
		end_error(false);
		cerr << "[TCP] Connection reset by peer\n";
		return;
	}
	if(!_receiver.ackno().has_value() && !seg.header().syn) {
		cerr << "[TCP debug] Bad segment! receiver is waiting for SYN, but segment has no SYN.\n";
		return;
	}
	_receiver.segment_received(seg);
	if(seg.header().ack) {
		_sender.ack_received(seg.header().ackno, seg.header().win);
	}
	if(seg.header().fin && _sender.next_seqno_absolute() < _sender.stream_in().bytes_written() + 2)
		_linger_after_streams_finish = false;
	_sender.fill_window();
	if(seg.length_in_sequence_space() && _sender.segments_out().empty())
		_sender.send_empty_segment();
	send_all();
}
void TCPConnection::send_segment(TCPSegment seg) {
	seg.header().ack = bool(_receiver.ackno());
	seg.header().ackno = *(_receiver.ackno());
	seg.header().win = _receiver.window_size();
	seg.header().rst = _sender.stream_in().error();
	_segments_out.push(seg);
	cerr << ">> SEGMENT SENT: header = " << seg.header().summary() << "queue size = " << _segments_out.size() << endl;
	cerr << "[TCPDEBUG] Incoming stream status: endinput? " << _sender.stream_in().input_ended() << " eof? "<<_sender.stream_in().eof() << endl;
}
void TCPConnection::send_all() {
	while(_sender.segments_out().size()) {
		TCPSegment fr = _sender.segments_out().front();
		_sender.segments_out().pop();
		send_segment(fr);
	}
	try_fin();
}

size_t TCPConnection::write(const string &data) {
	cerr << "[TCP debug] write \"" << data << "\", sz = " << data.length() << "\n";
	if(!active()) return 0;
	size_t ret = _sender.stream_in().write(data);
	_sender.fill_window();
	send_all();
	return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
	if(_sender.next_seqno_absolute() == 0U || !_active) return;
	_time_since_last += ms_since_last_tick;
	_sender.tick(ms_since_last_tick);
	_sender.fill_window();
	send_all();
	if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
		end_error(true);
		return;
	}
	try_fin();
}

void TCPConnection::connect() {
	cerr << "[TCP debug] connect\n";
	_sender.fill_window();
	send_all();
}

TCPConnection::~TCPConnection() {
	cerr << "[TCP debug] TCP destroyed\n";
	cerr << "[TCPDEBUG] Incoming stream status: endinput? " << _sender.stream_in().input_ended() << " eof? "<<_sender.stream_in().eof() << endl;
    try {
        if (active()) {
            cerr << "[TCP] Warning: Unclean shutdown of TCPConnection in destruction function\n";
			end_error(true);
            // Your code here: need to send a RST segment to the peer
        } else cerr << "[TCP] OK, inactive when destruction.\n";
    } catch (const exception &e) {
        std::cerr << "[TCP] Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::end_error(bool sendRst) {
	cerr << "END ERROR!" << endl;
	std::queue<TCPSegment>().swap(_segments_out);
	if(sendRst) {
		_sender.send_empty_segment();
		TCPSegment rst = _sender.segments_out().front();
		_sender.segments_out().pop();
		rst.header().rst = true;
		_segments_out.push(rst);
		cerr << "Rst sent\n";
		cerr << ">> SEGMENT SENT: header = " << rst.header().summary() << ", queue size = " << _segments_out.size() << endl;
	cerr << "[TCPDEBUG] Incoming stream status: endinput? " << _sender.stream_in().input_ended() << " eof? "<<_sender.stream_in().eof() << endl;
	}
	_active = false;
	_sender.stream_in().set_error();
	_receiver.stream_out().set_error();
}
void TCPConnection::try_fin() {
	if(!_active) return;
	if(_receiver.unassembled_bytes() > 0U || !_receiver.stream_out().eof() || !_sender.fin_acked())
		return;
	if(!_linger_after_streams_finish || _time_since_last >= 10*_cfg.rt_timeout) {
		cerr << "[TCP debug] OK, ";
		if(!_linger_after_streams_finish) cerr << "peer closed first, I don't need to linger.\n";
		else cerr << "linger time >= 10*rt_timeout. I can assume that peer has already closed\n";
		_active = false;
	}
}
void TCPConnection::end_input_stream() {
	cerr << "[TcP debug] end input stream\n";
	_sender.stream_in().end_input();
	_sender.fill_window();
	send_all();
}
