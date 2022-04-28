#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    EthernetFrame frame;

    //set ethernet header
    frame.header().src = _ethernet_address;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.payload() = dgram.serialize();


    auto it = _arp_cache.find(next_hop_ip);
    if(it == _arp_cache.end()){
        _queued_frames.insert({next_hop_ip,frame});
        send_arp_packet({},next_hop_ip,ARPMessage::OPCODE_REQUEST);
        return;
    }
    EthernetAddress target_ethernet_address = it->second;
    frame.header().dst = target_ethernet_address;
    _frames_out.push(std::move(frame));


}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address){
        return std::nullopt;
    }

    if(frame.header().type==EthernetHeader::TYPE_IPv4){
        InternetDatagram dgram;
        dgram.parse(frame.payload());
        return dgram;
    }
    parse_arp_message(frame);
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _current_time += ms_since_last_tick;

    // keep cached arp info for 30 seconds
    while(not _arp_queue.empty()){
        if(_arp_queue.front().second + 30000 >= _current_time)break;
        _arp_cache.erase(_arp_queue.front().first);
        _arp_queue.pop();
    }

}

void NetworkInterface::send_arp_packet(EthernetAddress target_address, uint32_t target_ip, uint16_t op){
    if(op==ARPMessage::OPCODE_REQUEST){
        auto it = _last_arp_request_time.find(target_ip);
        if(it != _last_arp_request_time.end() && it->second + 5000 > _current_time){
            return;
        }
        _last_arp_request_time[target_ip] = _current_time;
    }

    ARPMessage message;
    message.hardware_type = ARPMessage::TYPE_ETHERNET;
    message.protocol_type = EthernetHeader::TYPE_IPv4;
    message.opcode = op;

    message.sender_ethernet_address = _ethernet_address;
    message.sender_ip_address = _ip_address.ipv4_numeric();

    message.target_ethernet_address = target_address;
    message.target_ip_address = target_ip;


    EthernetFrame frame;
    frame.header().src = _ethernet_address;
    frame.header().dst = op==ARPMessage::OPCODE_REQUEST ?  ETHERNET_BROADCAST : target_address;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.payload() = message.serialize();
    _frames_out.push(frame);

}

void NetworkInterface::parse_arp_message(EthernetFrame frame) {
    ARPMessage arp_message;
    arp_message.parse(frame.payload());

    EthernetAddress dst_address = arp_message.target_ethernet_address;
    // ignore message with dst address is not BROADCAST or local ethernet address.
    if(dst_address != EthernetAddress{} && dst_address != _ethernet_address){
        return;
    }
    if(arp_message.opcode == ARPMessage::OPCODE_REQUEST){
        if(arp_message.target_ip_address == _ip_address.ipv4_numeric())
            send_arp_packet(arp_message.sender_ethernet_address,arp_message.sender_ip_address,ARPMessage::OPCODE_REPLY);
    }


    uint32_t src_ip = arp_message.sender_ip_address;
    EthernetAddress src_address = arp_message.sender_ethernet_address;

    //already exist
    if(_arp_cache.find(src_ip) != _arp_cache.end()){
        return;
    }

    _arp_cache[src_ip] = src_address;
    _arp_queue.push({src_ip,_current_time});

    send_queued_frames(src_ip);
}
void NetworkInterface::send_queued_frames(uint32_t ip) {
    auto eaddr_it = _arp_cache.find(ip);
    if(eaddr_it== _arp_cache.end()){
        cerr << "unexpected situation: send_queued_frames but ethernet address not cached\n";
        return;
    }

    auto frames_it = _queued_frames.find(ip);

    while(frames_it != _queued_frames.end() && frames_it->first==ip){
        frames_it->second.header().dst = eaddr_it->second;
        _frames_out.push(std::move(frames_it->second));
        auto _del = frames_it;
        ++frames_it;
        _queued_frames.erase(_del);
    }

}
