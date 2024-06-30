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

#include <argparse/argparse.hpp>

#include "client.h"
#include "peer.h"
#include "message.h"
#include "metainfo.h"
#include "tracker_protocol.h"
#include "tcp_utils.h"
#include "file.h"

int main(int argc, char *argv[])
{
    // get arguments from command line

    argparse::ArgumentParser program("client");
    std::string torrent_file;
    std::string client_id;
    int port;
    int timeout;
    int request_queue_size;
    std::string out_file;

    program.add_argument("-f").default_value("debian1.torrent").store_into(torrent_file);
    program.add_argument("-id").default_value("EZ6969").store_into(client_id);
    program.add_argument("-p").default_value(6881).store_into(port);
    program.add_argument("-t").default_value(120 * 1000).store_into(timeout); // default timeout to 2 minutes
    program.add_argument("-q").default_value(10).store_into(request_queue_size);
    program.add_argument("-o").default_value(torrent_file + ".out").store_into(out_file);

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

    // generate unique peer id
    std::string peer_id = Client::unique_peer_id(client_id);

    // send to tracker, get peers
    TrackerProtocol::TrackerManager tracker(torrent_file, peer_id, port);
    tracker.send_http();
    tracker.recv_http();

    // get all base peers from the tracker
    std::vector<Peer::PeerClient> peers = tracker.peers;
    int fd_count = peers.size();
    pollfd *pfds = new pollfd[fd_count];

    std::cout << "Got " << fd_count << " peers." << std::endl;

    // get info hash
    std::string metainfo_buffer = Metainfo::read_metainfo_to_buffer(torrent_file);
    std::string info_dict_str = Metainfo::read_info_dict_str(metainfo_buffer);
    std::string info_hash = Metainfo::hash_info_dict_str(info_dict_str);

    File::SingleFileTorrent torrent = File::SingleFileTorrent(metainfo_buffer, out_file);

    // create socket for all peers, and set to nonblocking
    // connect on these sockets
    for (int i = 0; i < fd_count; i++)
    {
        // create and set socket to be non blocking
        int peer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        fcntl(peer_sock, F_SETFL, O_NONBLOCK);

        pfds[i].fd = peer_sock;
        pfds[i].events = POLLIN | POLLOUT;
        peers[i].socket = peer_sock;
        int connect_ret = connect(peer_sock, (sockaddr *)(&peers[i].sockaddr), sizeof(sockaddr_in));

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
        int poll_peers = poll(pfds, fd_count, timeout);

        // no peers responded
        // should send another request to tracker
        if (poll_peers == 0)
        {
            break;
        }

        for (int i = 0; i < fd_count; i++)
        {

            // any client that responded is now connected
            // now we need to handshake

            // ready to read on the peer socket
            if (pfds[i].revents & POLLIN)
            {

                peers[i].connected |= true;
                int bytes_recv = 0;

                // peek the recv to see if the connection has closed
                uint8_t dummy_byte[4];
                int peek_recv = recv(pfds[i].fd, &dummy_byte, 4, MSG_PEEK);

                // error occurred on the peer client
                if (peek_recv == -1)
                {
                    std::cout << "Recv failed: " << strerror(errno) << std::endl;
                    // stop polling the socket
                    pfds[i].fd = -1;
                }

                // socket has closed by the peer client
                else if (peek_recv == 0)
                {
                    std::cout << "peer connection closed: " << i << std::endl;
                    // stop polling this socket
                    pfds[i].fd = -1;
                }

                // the peer is sending a continuation of a previous message
                // place the new bytes in the previously made buffer, and continue
                else if (peers[i].reading)
                {
                    uint32_t total_len = (peers[i].buffer)->total_length;
                    uint32_t bytes_read = (peers[i].buffer)->bytes_read;

                    // request the remaining number of bytes
                    // place into ptr + bytes_read
                    bytes_recv = recv(pfds[i].fd, ((peers[i].buffer)->ptr).get() + bytes_read, total_len - bytes_read, 0);
                    peers[i].buffer->bytes_read += bytes_recv;
                }

                // peer is sending a new handshake message
                else if (!peers[i].recv_shake)
                {
                    uint8_t pstrlen;

                    // recv first byte. Note that because we peeked, this should give > 0 bytes
                    bytes_recv = recv(pfds[i].fd, &pstrlen, 1, 0);

                    // create a buffer for the handshake length
                    peers[i].buffer = new Messages::Buffer(pstrlen);

                    // write pstrlen into the buffer
                    memcpy(peers[i].buffer->ptr.get(), &pstrlen, sizeof(pstrlen));
                    (peers[i].buffer)->bytes_read += bytes_recv;

                    // recv for rest of the bytes
                    bytes_recv = recv(pfds[i].fd, peers[i].buffer->ptr.get() + sizeof(pstrlen), 49 + pstrlen - sizeof(pstrlen), 0);
                    peers[i].buffer->bytes_read += bytes_recv;
                    peers[i].reading |= true;
                }

                // peer is sending other new messages...
                // read the first 4 bytes, alloc the buffer, try to recv
                else if (peers[i].sent_shake && peers[i].recv_shake)
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

                        peers[i].buffer = new Messages::Buffer(len + 4); // need to include the length field

                        // move the pointer when we write
                        int idx = 0;
                        memcpy(peers[i].buffer->ptr.get() + idx, &len, sizeof(len)); // write len into the buffer
                        idx += sizeof(len);

                        memcpy(peers[i].buffer->ptr.get() + idx, &id, sizeof(id)); // write id into the buffer
                        idx += sizeof(id);

                        (peers[i].buffer)->bytes_read += idx;

                        // recv the payload if theres more
                        bytes_recv = recv(pfds[i].fd, peers[i].buffer->ptr.get() + idx, len - sizeof(id), 0);
                        peers[i].buffer->bytes_read += bytes_recv;
                        peers[i].reading |= true;
                    }
                }

                // if the recv we did for the peer resulted in a completed message.
                // if so, then unpack the message, free the buffer, set reading to false.
                bool done_recv = (peers[i].buffer) && (peers[i].buffer)->bytes_read == (peers[i].buffer)->total_length;
                if (done_recv)
                {
                    uint8_t id_in_buffer;
                    // if no handshake yet, then this must be a handshake message
                    if (!peers[i].recv_shake)
                    {
                        Messages::Handshake peer_handshake = Messages::Handshake(peers[i].buffer);
                        peers[i].recv_shake = true;
                        peers[i].peer_id = peer_handshake.get_peer_id();
                        std::cout << "Handshake successful with: " << peers[i].peer_id << std::endl;
                    }

                    // other messages
                    else
                    {
                        // skip the len field, and read the id
                        memcpy(&id_in_buffer, peers[i].buffer->ptr.get() + 4, 1);

                        switch (id_in_buffer)
                        {
                        case Messages::CHOKE_ID:
                        {
                            peers[i].peer_choking = true;
                            std::cout << "got choke" << std::endl;
                            break;
                        }

                        case Messages::UNCHOKE_ID:
                        {
                            peers[i].peer_choking = false;
                            std::cout << "got unchoke" << std::endl;
                            break;
                        }
                        case Messages::INTERESTED_ID:
                        {
                            peers[i].peer_interested = true;
                            std::cout << "got interested" << std::endl;
                            break;
                        }
                        case Messages::NOTINTERESTED_ID:
                        {
                            peers[i].peer_interested = false;
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
                            peers[i].peer_bitfield = new File::BitField(peers[i].buffer);
                            break;
                        }

                        case Messages::REQUEST_ID:
                        {
                            std::cout << "got request" << std::endl;
                            break;
                        }
                        case Messages::PIECE_ID:
                        {
                            // std::cout << "got piece" << std::endl;
                            torrent.write_block(peers[i].buffer);
                            break;
                        }
                        }
                    }

                    // clear out buffer
                    delete peers[i].buffer;
                    peers[i].buffer = nullptr;
                    peers[i].reading = false;
                }
            }

            if (pfds[i].revents & POLLOUT)
            {
                // verify that we're connected
                int error = 0;
                socklen_t len = sizeof(error);
                int retval = getsockopt(pfds[i].fd, SOL_SOCKET, SO_ERROR, &error, &len);

                // if we haven't handshake with this peer yet, send handshake
                if (retval == 0 && !peers[i].sent_shake)
                {
                    Messages::Handshake client_handshake = Messages::Handshake(19, "BitTorrent protocol", info_hash, client_id);
                    Messages::Buffer *buff = client_handshake.pack();

                    // send the buffer over the wire
                    uint32_t remaining_bytes = buff->total_length;
                    assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);
                    delete buff;

                    peers[i].sent_shake = true;
                }

                // if we arent already interested in this peer, see if we got their bitfield
                // then check if they have any pieces we need. If they do, then send an interested message
                else if (retval == 0 && peers[i].peer_bitfield != nullptr && !peers[i].am_interested)
                {
                    int match_idx = torrent.piece_bitfield->first_match(peers[i].peer_bitfield);

                    // we need a piece from this peer, so send interested
                    if (match_idx != -1)
                    {
                        Messages::Interested interested_msg = Messages::Interested();
                        Messages::Buffer *buff = interested_msg.pack();

                        uint32_t remaining_bytes = buff->total_length;
                        assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                        delete buff;
                        peers[i].am_interested = true;

                        std::cout << "sent interested" << std::endl;
                    }
                }

                // if we are interested in this peer, see if they still have any pieces we need
                // if they dont, update by sending not interested. If they do, send requests if we arent choked
                else if (retval == 0 && peers[i].peer_bitfield != nullptr && peers[i].am_interested && !peers[i].peer_choking)
                {
                    int match_idx = torrent.piece_bitfield->first_match(peers[i].peer_bitfield);

                    // we should be notinterested, so sent this message to our peer
                    if (match_idx == -1)
                    {
                        Messages::NotInterested notinterested_msg = Messages::NotInterested();
                        Messages::Buffer *buff = notinterested_msg.pack();

                        uint32_t remaining_bytes = buff->total_length;
                        assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                        delete buff;
                        peers[i].am_interested = false;
                        std::cout << "sent notinterested" << std::endl;
                    }

                    // peer still has pieces we need, so send requests (number of reqs based on args)
                    else
                    {
                        int num_requests = 0;

                        // if block queue is empty, try to refresh
                        // this might overload peer with requests
                        
                        if (torrent.block_queue.empty())
                        {
                            torrent.update_block_queue();
                        }
                        

                        // send at most request_queue_size requests
                        while (!torrent.block_queue.empty() && num_requests < request_queue_size)
                        {
                            File::Block block = torrent.block_queue.front();
                            Messages::Request request = Messages::Request(block.index, block.begin, block.length);
                            Messages::Buffer *buff = request.pack();
                            uint32_t remaining_bytes = buff->total_length;
                            assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);

                            std::cout << "sent request: " << block.to_string() << std::endl;

                            delete buff;
                            num_requests++;
                            torrent.block_queue.pop();
                        }
                    }
                    
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
        }

        // check if the torrent is done, all pieces in piece bitfield are flipped
        // if they are, write the file to the output
        if (torrent.piece_bitfield->all_flipped())
        {
            std::cout << "done with torrent" << std::endl;
            break;
        }
    }

    delete[] pfds;

    return 0;
}