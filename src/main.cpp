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

int main(int argc, char *argv[])
{
    // get arguments from command line

    argparse::ArgumentParser program("client");
    std::string torrent_file;
    std::string client_id;
    int port;

    program.add_argument("-f").default_value("debian1.torrent").store_into(torrent_file);
    program.add_argument("-id").default_value("EZ6969").store_into(client_id);
    program.add_argument("-p").default_value(6881).store_into(port);
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

    TrackerProtocol::TrackerManager tracker(torrent_file, peer_id, port);
    tracker.send_http();
    tracker.recv_http();

    // get all base peers from the tracker
    std::vector<Peer::PeerClient> peers = tracker.peers;
    int fd_count = peers.size();
    pollfd *pfds = new pollfd[fd_count];

    // get info hash
    std::string metainfo_buffer = Metainfo::read_metainfo(torrent_file);
    std::string info_dict_str = Metainfo::read_info_dict_str(metainfo_buffer);
    std::string info_hash = Metainfo::hash_info_dict_str(info_dict_str);

    // create socket for all peers, and set to nonblocking
    // connect on these sockets
    for (int i = 0; i < fd_count; i++)
    {
        int peer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        fcntl(peer_sock, F_SETFL, O_NONBLOCK);
        pfds[i].fd = peer_sock;
        pfds[i].events = POLLIN | POLLOUT;
        peers[i].socket = peer_sock;
        int connect_ret = connect(peer_sock, (sockaddr *)(&peers[i].sockaddr), sizeof(sockaddr_in));

        // connecting on non blocking sockets should give EINPROGRESS
        if (errno != EINPROGRESS)
        {
            std::cout << strerror(errno) << std::endl;
        }
    }

    // main poll event loop
    while (true)
    {
        int poll_peers = poll(pfds, fd_count, 5 * 1000);

        for (int i = 0; i < fd_count; i++)
        {

            // any client that responded is now connected
            // now we need to handshake
            if (pfds[i].revents & POLLOUT)
            {
                // verify that we're connected
                int error = 0;
                socklen_t len = sizeof(error);
                int retval = getsockopt(pfds[i].fd, SOL_SOCKET, SO_ERROR, &error, &len);

                if (retval == 0 && !peers[i].sent_shake)
                {

                    // if we haven't handshake with this peer yet, send handshake
                    Messages::Handshake client_handshake = Messages::Handshake(19, "BitTorrent protocol", info_hash, client_id);

                    // pack the handshake into a buffer, then send over the wire
                    Messages::Buffer *buff = client_handshake.pack();
                    uint32_t remaining_bytes = buff->total_length;

                    assert(sendall(pfds[i].fd, (buff->ptr).get(), &remaining_bytes) >= 0);
                    delete buff;

                    peers[i].sent_shake = true;
                }

                if (retval != 0)
                {
                    std::cout << strerror(retval) << std::endl;
                }

                if (error != 0)
                {
                    std::cout << strerror(error) << std::endl;
                }
            }

            // ready to read on the peer socket
            if (pfds[i].revents & POLLIN)
            {

                peers[i].connected |= true;
                int bytes_recv = 0;

                // peek the recv to see if the connection has closed
                uint8_t dummy_byte;
                int peek_recv = recv(pfds[i].fd, &dummy_byte, 1, MSG_PEEK);

                // error occurred on the peer client
                if (peek_recv == -1)
                {
                    std::cout << strerror(errno) << std::endl;
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

                    // write pstrlen into the buffer (we assume that the full message will be in the buffer)
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
                    std::cout << "new message" << std::endl;
                }

                // see if the recv we did for the peer resulted in a completed message
                // if so, then unpack the message, free the buffer,
                // set reading to false.

                // note that the peer buffer must be non-null for a message to be fulfilled
                // otherwise, this means that we didn't process the message when we received it
                bool done_recv = (peers[i].buffer) && (peers[i].buffer)->bytes_read == (peers[i].buffer)->total_length;
                if (done_recv)
                {
                    // if no handshake yet, then this must be a handshake message
                    if (!peers[i].recv_shake)
                    {
                        Messages::Handshake peer_handshake = Messages::Handshake();
                        peer_handshake.unpack(peers[i].buffer);

                        peers[i].recv_shake = true;
                        peers[i].peer_id = peer_handshake.get_peer_id();
                    }

                    // other messages... find using id field!

                    // clear out buffer
                    delete peers[i].buffer;
                    peers[i].buffer = nullptr;
                    peers[i].reading = false;
                }
            }
        }
    }

    delete[] pfds;

    return 0;
}