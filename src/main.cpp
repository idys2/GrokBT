#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <queue>

#include <argparse/argparse.hpp>

#include "client.h"
#include "peer.h"
#include "message.h"
#include "metainfo.h"
#include "tracker_protocol.h"
#include "net_utils.h"
#include "file.h"
#include "hash.h"

int main(int argc, char *argv[])
{
    // get arguments from command line
    argparse::ArgumentParser program("client");
    std::string torrent_file;
    std::string client_id;
    int port;
    int timeout;
    int outgoing_request_queue_size;
    int incoming_request_queue_size;
    int listen_queue_size;

    int newfd;
    sockaddr_storage remoteaddr;
    socklen_t addrlen;
    sockaddr_in self_addr;

    program.add_argument("-f").default_value("debian1.torrent").store_into(torrent_file);
    program.add_argument("-id").default_value("EZ6969").store_into(client_id);
    program.add_argument("-p").default_value(6881).store_into(port);
    program.add_argument("-t").default_value(120 * 1000).store_into(timeout); // default timeout to 2 minutes
    program.add_argument("-oq").default_value(10).store_into(outgoing_request_queue_size);
    program.add_argument("-iq").default_value(30).store_into(incoming_request_queue_size);
    program.add_argument("-lq").default_value(20).store_into(listen_queue_size);

    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    };

    // get our address
    self_addr = get_self_sockaddr(port);

    // send to tracker, get peers
    TrackerProtocol::TrackerManager tracker(torrent_file, Client::unique_peer_id(client_id), port);
    tracker.send_http();
    tracker.recv_http();

    // request queue for seeding
    std::queue<File::Block> requests;

    // get all base peers from the tracker
    std::vector<Peer::PeerClient> peers = tracker.peers;

    // remove this client from the peers list
    // we assume that the peers list contains peers that do not include ourselves
    peers.erase(std::remove_if(peers.begin(), peers.end(), [&](Peer::PeerClient peer)
                               { return peer.sockaddr.sin_addr.s_addr == self_addr.sin_addr.s_addr &&
                                        peer.sockaddr.sin_port == self_addr.sin_port; }),
                peers.end());

    std::cout << "Got: " << peers.size() << " peers " << std::endl;

    // remove self from the peer list
    int fd_count = peers.size() + 1;

    // set up poll vector
    std::vector<pollfd> pfds = std::vector<pollfd>(fd_count);

    // get 20 byte info hash
    std::string metainfo_buffer = Metainfo::read_metainfo_to_buffer(torrent_file);
    std::string info_dict_str = Metainfo::read_info_dict_str(metainfo_buffer);
    std::string info_hash = Hash::truncated_sha1_hash(info_dict_str, 20);
    File::SingleFileTorrent torrent = File::SingleFileTorrent(metainfo_buffer);

    // get listener socket
    int listener = get_listener_socket(port, listen_queue_size);
    fcntl(listener, F_SETFL, O_NONBLOCK);

    // add the listener socket
    pfds[0].fd = listener;
    pfds[0].events = POLLIN;

    // create socket for all peers, and set to nonblocking
    // connect on these sockets
    for (int i = 1; i < pfds.size(); i++)
    {
        // create and set socket to be non blocking
        int peer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        fcntl(peer_sock, F_SETFL, O_NONBLOCK);

        pfds[i].fd = peer_sock;
        pfds[i].events = POLLIN | POLLOUT;
        peers[i - 1].socket = peer_sock;
        int connect_ret = connect(peer_sock, (sockaddr *)(&peers[i - 1].sockaddr), sizeof(sockaddr_in));

        // connecting on non blocking sockets should give EINPROGRESS
        if (errno != EINPROGRESS)
        {
            std::cout << "Nonblocking connect failed" << std::endl;
            std::cout << strerror(errno) << std::endl;
        }
    }

    // main poll event loop
    while (true)
    {
        int poll_peers = poll(pfds.data(), pfds.size(), timeout);

        // no peers responded
        // should send another request to tracker

        for (int i = 0; i < pfds.size(); i++)
        {
            int peer_idx = i - 1;

            if (pfds[i].revents & POLLOUT)
            {
                // verify that we're connected
                int error = 0;
                socklen_t len = sizeof(error);
                int retval = getsockopt(pfds[i].fd, SOL_SOCKET, SO_ERROR, &error, &len);

                // if we haven't handshake with this peer yet, send handshake
                if (retval == 0 && !peers[peer_idx].sent_shake)
                {
                    Messages::Handshake client_handshake = Messages::Handshake(19, "BitTorrent protocol", info_hash, client_id);
                    Messages::Buffer *buff = client_handshake.pack();

                    // send the buffer over the wire
                    uint32_t remaining_bytes = buff->total_length;
                    assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);
                    delete buff;

                    peers[peer_idx].sent_shake = true;
                }

                // if we arent already interested in this peer, see if we got their bitfield
                // then check if they have any pieces we need. If they do, then send an interested message
                else if (retval == 0 && peers[peer_idx].peer_bitfield != nullptr && !peers[peer_idx].am_interested)
                {
                    int match_idx = torrent.piece_bitfield->first_match(peers[peer_idx].peer_bitfield);

                    // we need a piece from this peer, so send interested
                    if (match_idx != -1)
                    {
                        Messages::Interested interested_msg = Messages::Interested();
                        Messages::Buffer *buff = interested_msg.pack();

                        uint32_t remaining_bytes = buff->total_length;
                        assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                        delete buff;
                        peers[peer_idx].am_interested = true;

                        std::cout << "sent interested" << std::endl;
                    }
                }

                // if we are interested in this peer, see if they still have any pieces we need
                // if they dont, update by sending not interested. If they do, send requests if we arent choked
                else if (retval == 0 && peers[peer_idx].am_interested && !peers[peer_idx].peer_choking)
                {
                    int match_idx = torrent.piece_bitfield->first_match(peers[peer_idx].peer_bitfield);

                    // we should be notinterested, so send this message to our peer
                    if (match_idx == -1)
                    {
                        Messages::NotInterested notinterested_msg = Messages::NotInterested();
                        Messages::Buffer *buff = notinterested_msg.pack();

                        uint32_t remaining_bytes = buff->total_length;
                        assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                        delete buff;
                        peers[peer_idx].am_interested = false;
                        std::cout << "sent notinterested" << std::endl;
                    }

                    // peer still has pieces we need, so send requests (number of reqs based on args)
                    // try to maintain 10 requests outgoing for this peer.
                    else
                    {
                        // if block queue is empty, try to refresh

                        if (torrent.block_queue.empty())
                        {
                            torrent.update_block_queue();
                        }

                        // send at most outgoing_request_queue_size requests
                        while (!torrent.block_queue.empty() && peers[peer_idx].outgoing_requests < outgoing_request_queue_size)
                        {
                            File::Block block = torrent.block_queue.front();
                            Messages::Request request = Messages::Request(block.index, block.begin, block.length);
                            Messages::Buffer *buff = request.pack();
                            uint32_t remaining_bytes = buff->total_length;
                            assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                            std::cout << "sent request: " << block.to_string() << std::endl;

                            delete buff;
                            peers[peer_idx].outgoing_requests++;
                            torrent.block_queue.pop();
                        }
                    }
                }

                else if (requests.size() < outgoing_request_queue_size && peers[peer_idx].am_choking)
                {
                    Messages::Unchoke unchoke_msg = Messages::Unchoke();
                    Messages::Buffer *buff = unchoke_msg.pack();

                    uint32_t remaining_bytes = buff->total_length;
                    assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                    delete buff;
                    peers[peer_idx].am_choking = false;
                    std::cout << "sent unchoke" << std::endl;
                }

                else if (requests.size() >= outgoing_request_queue_size && !peers[peer_idx].am_choking) {
                    Messages::Choke choke_msg = Messages::Choke();
                    Messages::Buffer *buff = choke_msg.pack();

                    uint32_t remaining_bytes = buff->total_length;
                    assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                    delete buff;
                    peers[peer_idx].am_choking = true;
                    std::cout << "sent choke" << std::endl;
                }

                // if peer refuses/resets the connection,
                // set their socket to not be polled
                if (retval != 0)
                {
                    std::cout << "getsockopt failed" << std::endl;
                    std::cout << strerror(retval) << std::endl;
                    pfds[i].fd = -1;
                }

                if (error != 0)
                {
                    std::cout << "getsockopt error" << std::endl;
                    std::cout << strerror(error) << std::endl;
                    pfds[i].fd = -1;
                }

                
            }

            // check if new connections can be made
            if (pfds[i].revents & POLLIN && pfds[i].fd == listener)
            {
                addrlen = sizeof(remoteaddr);
                newfd = accept(listener, (sockaddr *)&remoteaddr, &addrlen);

                // add the new connection to our list
                if (newfd != -1)
                {
                    pfds.push_back(pollfd());
                    pfds[pfds.size() - 1].fd = newfd;
                    pfds[pfds.size() - 1].events = POLLIN | POLLOUT;
                    // add a new peer object
                    peers.push_back(Peer::PeerClient());
                    peers[peers.size() - 1].socket = newfd;
                }
            }

            // ready to read on the peer socket
            else if (pfds[i].revents & POLLIN)
            {
                int bytes_recv = 0;

                // peek the recv to see if the connection has closed
                uint8_t dummy_byte[4];
                int peek_recv = recv(pfds[i].fd, &dummy_byte, 4, MSG_PEEK);

                // error occurred on the peer client
                if (peek_recv == -1)
                {
                    std::cout << "Recv failed: " << strerror(errno) << std::endl;
                    std::cout << peers[peer_idx].to_string() << std::endl;

                    // stop polling the socket
                    pfds[i].fd = -1;
                }

                // socket has closed by the peer client
                else if (peek_recv == 0)
                {
                    std::cout << "peer connection closed: " << i << std::endl;
                    pfds[i].fd = -1;
                }

                // the peer is sending a continuation of a previous message
                // place the new bytes in the previously made buffer, and continue
                else if (peers[peer_idx].reading)
                {
                    uint32_t total_len = (peers[peer_idx].buffer)->total_length;
                    uint32_t bytes_read = (peers[peer_idx].buffer)->bytes_read;

                    // request the remaining number of bytes
                    // place into ptr + bytes_read
                    bytes_recv = recv(pfds[i].fd, ((peers[peer_idx].buffer)->ptr).get() + bytes_read, total_len - bytes_read, 0);
                    peers[peer_idx].buffer->bytes_read += bytes_recv;
                }

                // peer is sending a new handshake message
                else if (!peers[peer_idx].recv_shake)
                {
                    uint8_t pstrlen;

                    // recv first byte. Note that because we peeked, this should give > 0 bytes
                    bytes_recv = recv(pfds[i].fd, &pstrlen, 1, 0);

                    // create a buffer for the handshake length
                    peers[peer_idx].buffer = new Messages::Buffer(pstrlen);

                    // write pstrlen into the buffer
                    memcpy(peers[peer_idx].buffer->ptr.get(), &pstrlen, sizeof(pstrlen));
                    (peers[peer_idx].buffer)->bytes_read += bytes_recv;

                    // recv for rest of the bytes
                    bytes_recv = recv(pfds[i].fd, peers[peer_idx].buffer->ptr.get() + sizeof(pstrlen), 49 + pstrlen - sizeof(pstrlen), 0);
                    peers[peer_idx].buffer->bytes_read += bytes_recv;
                    peers[peer_idx].reading |= true;
                }

                // peer is sending other new messages...
                // read the first 4 bytes, alloc the buffer, try to recv
                else if (peers[peer_idx].sent_shake && peers[peer_idx].recv_shake)
                {
                    uint32_t len;
                    uint8_t id;
                    bytes_recv = recv(pfds[i].fd, &len, sizeof(len), 0);
                    len = ntohl(len); // len is in big endian, so convert

                    // non keep alive message
                    // note that any fields that we write here will be in big endian, so we need to convert
                    // downstream.
                    if (len > 0 && bytes_recv > 0)
                    {
                        bytes_recv = recv(pfds[i].fd, &id, sizeof(id), 0);

                        peers[peer_idx].buffer = new Messages::Buffer(len + 4); // need to include the length field

                        // move the pointer when we write
                        int idx = 0;
                        memcpy(peers[peer_idx].buffer->ptr.get() + idx, &len, sizeof(len)); // write len into the buffer
                        idx += sizeof(len);

                        memcpy(peers[peer_idx].buffer->ptr.get() + idx, &id, sizeof(id)); // write id into the buffer
                        idx += sizeof(id);

                        (peers[peer_idx].buffer)->bytes_read += idx;

                        // recv the payload if theres more
                        bytes_recv = recv(pfds[i].fd, peers[peer_idx].buffer->ptr.get() + idx, len - sizeof(id), 0);
                        peers[peer_idx].buffer->bytes_read += bytes_recv;
                        peers[peer_idx].reading |= true;
                    }
                }

                // if the recv we did for the peer resulted in a completed message.
                // if so, then unpack the message, free the buffer, set reading to false.
                bool done_recv = (peers[peer_idx].buffer) && (peers[peer_idx].buffer)->bytes_read == (peers[peer_idx].buffer)->total_length;
                if (done_recv)
                {
                    uint8_t id_in_buffer;
                    // if no handshake yet, then this must be a handshake message
                    if (!peers[peer_idx].recv_shake)
                    {
                        Messages::Handshake peer_handshake = Messages::Handshake(peers[peer_idx].buffer);
                        peers[peer_idx].recv_shake = true;
                        peers[peer_idx].peer_id = peer_handshake.get_peer_id();
                        std::cout << "Handshake: " << peer_handshake.get_peer_id() << std::endl;

                        // if info hash does not match, then we need to close
                        if (peer_handshake.get_info_hash() != info_hash)
                        {
                            pfds[peer_idx].fd = -1;
                            peers[peer_idx].recv_shake = false;
                            close(pfds[peer_idx].fd);
                            std::cout << "Handshake failed" << std::endl;
                        }

                        // on successful handshake, send our bitfield if we have pieces
                        else if (torrent.piece_bitfield->first_unflipped() != 0){
                            Messages::Buffer *buff = torrent.piece_bitfield->pack();
                            // send the buffer over the wire
                            uint32_t remaining_bytes = buff->total_length;
                            assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);
                            delete buff;

                            std::cout << "sent bitfield" << std::endl;
                        }
                    }

                    // other messages
                    else
                    {
                        // skip the len field, and read the id
                        memcpy(&id_in_buffer, peers[peer_idx].buffer->ptr.get() + 4, 1);

                        switch (id_in_buffer)
                        {
                        case Messages::CHOKE_ID:
                        {
                            peers[peer_idx].peer_choking = true;
                            std::cout << "got choke" << std::endl;
                            break;
                        }

                        case Messages::UNCHOKE_ID:
                        {
                            peers[peer_idx].peer_choking = false;
                            std::cout << "got unchoke" << std::endl;
                            break;
                        }
                        case Messages::INTERESTED_ID:
                        {
                            peers[peer_idx].peer_interested = true;
                            std::cout << "got interested" << std::endl;
                            break;
                        }
                        case Messages::NOTINTERESTED_ID:
                        {
                            peers[peer_idx].peer_interested = false;
                            std::cout << "got notinterested" << std::endl;
                            break;
                        }
                        case Messages::HAVE_ID:
                        {
                            std::cout << "got have" << std::endl;
                            break;
                        }
                        case Messages::BITFIELD_ID:
                        {
                            std::cout << "got bitfield" << std::endl;
                            peers[peer_idx].peer_bitfield = new File::BitField(peers[peer_idx].buffer, torrent.num_pieces);
                            break;
                        }

                        // Upon request, serve the piece to the client
                        case Messages::REQUEST_ID:
                        {
                            std::cout << "got request" << std::endl;
                            // parse buffer
                            Messages::Request req = Messages::Request(peers[peer_idx].buffer);
                            Messages::Buffer *buff = torrent.get_piece(req.length, req.begin, req.length);
                            // send the buffer over the wire
                            uint32_t remaining_bytes = buff->total_length;
                            assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);
                            delete buff;
                            break;
                        }

                        // Upon getting a piece, parse and write the data to our output
                        case Messages::PIECE_ID:
                        {
                            peers[peer_idx].outgoing_requests--;
                            torrent.write_block(peers[peer_idx].buffer);
                            break;
                        }
                        }
                    }

                    // clear out buffer
                    delete peers[peer_idx].buffer;
                    peers[peer_idx].buffer = nullptr;
                    peers[peer_idx].reading = false;
                }
            }
        }

        // check if the torrent is done, all pieces in piece bitfield are flipped
        if (torrent.piece_bitfield->all_flipped())
        {
            if (!tracker.sent_completed)
            {
                std::cout << "done with torrent" << std::endl;
                // send a completed message
                tracker.event = TrackerProtocol::EventType::COMPLETED;
                tracker.downloaded = std::to_string(torrent.downloaded);
                tracker.uploaded = std::to_string(torrent.uploaded);
                tracker.left = std::to_string(torrent.length - torrent.downloaded);
                tracker.send_http();
                tracker.sent_completed = true;
            }
        }
    }

    return 0;
}