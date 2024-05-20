#ifndef _CLIENT_H
#define _CLIENT_H

#include <errno.h>
#include <map>
#include <vector>
#include <chrono>
#include <queue>
#include <mutex>

#include "tracker_protocol.h"
#include "peer_wire_protocol.h"
#include "protocol_utils.h"

bool DEBUG = true;
#define DEBUGPRINTLN(x) if (DEBUG) std::cout << x << std::endl
#define on_error(x) {std::cerr << x << ": " << strerror(errno) << std::endl; exit(EXIT_FAILURE); }

namespace BitTorrentClient {

    //const int MAX_BATCH_REQUESTS = 20;
    

    class Torrent {

        public: 
            // peers maps the file descriptor of a peer to a struct containing information about that peer 
            // all peers added to this map are ensured to be connected to the respective filedescriptor, 
            // and have already completed the handshake with this client

            //std::map<int, std::string> fds;
            std::map<int, struct ProtocolUtils::Peer> peers;  // socket file descriptor -> Peer
            //std::mutex map_mutex;

            std::string torrent_file;   // the name of the torrent file
            std::string info_hash;      // the SHA1 20 byte truncated hash of the info dictionary
            
            // progress variables
            int uploaded;       // the total number of bytes uploaded (as a seeder) since start event 
            int downloaded;     // the total number of bytes downloaded (as a leecher) since start event
            int left;           // the number of bytes needed until we finish the torrent

            // tracker configs 
            int compact = 1;        // we will send compact requests
            int interval;           // the interval in seconds we should send regular requests to tracker
            std::string tracker_id; // this is only set if the tracker sent a tracker id

            // network parameters
            std::string client_id;  // the client's chosen ID for this torrent
            int client_port;        // the port that the client listens on
            int tracker_sock;       // the file descriptor for the socket used to communicate with tracker
            std::string tracker_announce_url; // announce_url of the tracker

            // information variables from tracker
            int num_seeders;
            int num_leechers;

            // torrent info dictionary
            ProtocolUtils::SingleFileInfo info_dict;  // information about the torrent (pieces, piece length, etc)

            // queue for batch sending messages
            //std::queue<PeerWireProtocol::Message> msg_queue;

            Torrent() {}

            Torrent(std::string filename, std::string cid, int port) {
                torrent_file = filename;

                std::string metainfo = ProtocolUtils::read_metainfo(torrent_file);
                std::string info_dict_str = ProtocolUtils::read_info_dict_str(metainfo); 

                info_dict = ProtocolUtils::read_info_dict(metainfo);
                info_hash = ProtocolUtils::hash_info_dict_str(info_dict_str);

                // get tracker info
                tracker_announce_url = ProtocolUtils::read_announce_url(metainfo);
                sockaddr_in tracker_addr = TrackerProtocol::get_tracker_addr(tracker_announce_url);

                tracker_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (tracker_sock < 0) on_error("Failed to create socket to tracker");

                int connected = connect(tracker_sock, (sockaddr *)&tracker_addr, sizeof(sockaddr_in));
                if (connected < 0) on_error("Failed to connect to tracker socket");    

                client_id = cid; 
                client_port = port;         

                // set initial values
                uploaded = 0; 
                downloaded = 0;
                left = info_dict.length;
            }

            /**
             * report on progress to tracker function,
             * uses fields of this Torrent to fill out a HTTP GET request sent to tracker
             * TODO: modify this function to send repeated tracker requests,
             * could maybe try to reuse tracker_request?, not really needed to recreate each time
             * @param[in]   event   event type enum
            */
            void report_to_tracker(TrackerProtocol::EventType event) {
                TrackerProtocol::TrackerRequest tracker_req(tracker_announce_url, tracker_sock);

                tracker_req.info_hash = info_hash;
                tracker_req.peer_id = client_id;
                tracker_req.port = client_port;
                tracker_req.uploaded = std::to_string(uploaded); 
                tracker_req.downloaded = std::to_string(downloaded); 
                tracker_req.left = std::to_string(left); 
                tracker_req.compact = compact;
                tracker_req.event = event;

                tracker_req.send_http();
            }

            // add new peers that the tracker gave
            // set all configs for talking to the tracker
            std::vector<ProtocolUtils::Peer> update_from_tracker(TrackerProtocol::TrackerResponse response) {
                
                interval = response.interval; 
                num_seeders = response.num_seeders;
                num_leechers = response.num_leechers;
                tracker_id = response.tracker_id;
                
                // peers we got from tracker
                std::vector<ProtocolUtils::Peer> new_peers = response.peers;

                /**
                 * new logic
                */

                // peers that haven't been recorded yet
                std::vector<ProtocolUtils::Peer> updated_new_peers = std::vector<ProtocolUtils::Peer>();
                
                for (auto& new_peer : new_peers) {
                    
                    // have to linearly scan to check if this new_peer is actually new
                    // TODO: can we do this better, not in O(n^2) time?
                    // idea: two maps: fd -> peer.str() and peer.str() -> Peer struct

                    bool found = false;
                    for(auto& peer_entry : peers) { 
                        auto peer = peer_entry.second;
                        
                        // this peer already exists
                        if(peer.str() == new_peer.str()) { 
                            found = true;
                            break; 
                        }
                    }
                    
                    // this peer is new, so try to add them to the map with a new socket, 
                    // and connect to them
                    if(!found) {
                        updated_new_peers.push_back(new_peer);
                    }
                }

                return updated_new_peers;

                /**
                 * old logic
                
                for (auto& new_peer : new_peers) {
                    
                    // have to linearly scan to check if this new_peer is actually new
                    // TODO: can we do this better, not in O(n^2) time?
                    // idea: two maps: fd -> peer.str() and peer.str() -> Peer struct

                    bool found = false;
                    for(auto& peer_entry : peers) { 
                        auto peer = peer_entry.second;
                        
                        // this peer already exists
                        if(peer.str() == new_peer.str()) { 
                            found = true;
                            break; 
                        }
                    }
                    
                    // this peer is new, so try to add them to the map with a new socket, 
                    // and connect to them
                    if(!found) {

                        
                        int peer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                        
                        if (peer_sock < 0) on_error("Failed to create socket for peer");

                        int connect_peer_sock = connect(peer_sock, (struct sockaddr *) &new_peer.sockaddr, sizeof(new_peer.sockaddr));
                        if (connect_peer_sock >= 0) {

                            // send handshake to peer
                            auto handshake = PeerWireProtocol::Handshake(info_hash, client_id);
                            handshake.send_msg(peer_sock);

                            // receive handshake
                            auto peer_handshake = PeerWireProtocol::Handshake(info_hash, client_id);
                            peer_handshake.recv_msg(peer_sock); 

                            // only add if this client is serving the correct hash
                            if(info_hash == peer_handshake.info_hash) {
                                peers.insert({peer_sock, new_peer});
                            }
                        }

                        // if we didn't create a socket or can't connect, then don't add to map
                        else {
                            std::cout << "failed to connect to " << new_peer.str() + ": " + strerror(errno) << std::endl;
                        }
                    } else DEBUGPRINTLN(new_peer.str() + " is already in the map");
                }
                
                */
            }

            // Send request to peer for a block
            void send_request(int pfd) { 
                
                // get the first piece index that isn't finished
                auto next_piece_idx = info_dict.piece_bitfield.first_unflipped();

                std::pair<uint32_t, uint32_t> block_pair = info_dict.piece_vec[next_piece_idx].get_next_request(); 
                auto begin_idx = std::get<0>(block_pair); 
                auto block_s = std::get<1>(block_pair);   

                PeerWireProtocol::Request req = PeerWireProtocol::Request(next_piece_idx, begin_idx, block_s);
                req.send_msg(pfd);
            }

            /**
             * Helper to send off all messages in the msg_queue, starting from the first msg
            */
            /*void send_msgs(int pfd) {
                while (!msg_queue.empty()) {
                    msg_queue.front().send_msg(pfd);
                    msg_queue.pop();
                }
            }*/
            
            // **************************************************************
            // Handler functions for receiving a type of message
            // **************************************************************

            /**
             * Receive these messages as a leecher
            */
            
            // When we receive a piece message, write this block of data and flip the block bitfield for this block
            void handle_piece(int pfd, PeerWireProtocol::Piece piece_resp) {
                uint32_t piece_index = piece_resp.piece_index; 
                uint32_t begin_index = piece_resp.begin_index;
                
                std::cout << "begin index: " << begin_index << ", piece_resp.piece_len: " << piece_resp.piece_len << std::endl;

                info_dict.piece_vec[piece_index].download_block(begin_index, piece_resp.piece_len, piece_resp.piece);

                if(info_dict.piece_vec[piece_index].is_done()) {
                    std::cout << "done with piece" << std::endl;
                    assert(info_dict.piece_vec[piece_index].verify_hash());

                    // flip and write to out
                    info_dict.piece_bitfield.set_bit(piece_index);

                    // write piece to disk
                    int start_byte_in_file = piece_index * info_dict.piece_length;
                    info_dict.out_file.seekp(start_byte_in_file, std::ios::beg);
                    info_dict.out_file.write(reinterpret_cast<char *>(info_dict.piece_vec[piece_index].output_buffer.data()), 
                                           info_dict.piece_vec[piece_index].output_buffer.size());
                    left -= info_dict.piece_vec[piece_index].output_buffer.size();
                }

                else {
                    std::cout << "not done with piece " << piece_index << std::endl;
                }

                if(!info_dict.piece_bitfield.all_flipped()) {
                    send_request(pfd);
                } else {
                    //wcprintf("Done downloading!\n");
                }
            }
            
            
            // peer is choking the us
            void handle_choke(int pfd) {
                peers.at(pfd).peer_choking = true;
            }

            // peer stopped choking us
            void handle_unchoke(int pfd) {
                peers.at(pfd).peer_choking = false; 

                // when peer unchokes us, we send an interested message, and begin sending requests
                // interested message is simply a notification to the seeder that we will begin sending requests
                PeerWireProtocol::Interested interested_msg = PeerWireProtocol::Interested();
                interested_msg.send_msg(pfd);
                peers.at(pfd).am_interested = true;
                
                // send first request msg
                DEBUGPRINTLN("Sending first request msg");
                send_request(pfd);
            }

            
            /**
             * new logic:
             * on unchoke, we can fill the queue with an interested message and then a bunch of requests,
             * then wait until the last request has been fufilled, and then fill the queue with a next bunch of requests and send 
            */
            /*void handle_unchoke2(int pfd) {
                peers.at(pfd).peer_choking = false; 

                msg_queue.push(PeerWireProtocol::Interested());

                int next_piece_idx = info_dict.piece_bitfield.first_unflipped();
                auto block_pairs = info_dict.piece_vec.at(next_piece_idx).get_next_n_requests(MAX_BATCH_REQUESTS);

                for (auto pair : block_pairs) {
                    auto begin_idx = std::get<0>(pair);
                    auto block_s = std::get<1>(pair);

                    msg_queue.push(PeerWireProtocol::Request(next_piece_idx, begin_idx, block_s));
                }

                send_msgs(pfd);
            }*/

            /**
             * Received messages as a seeder
            */

            // peer is interested in us
            void handle_interested(int pfd) {
                peers.at(pfd).peer_interested = true;
            }

            // peer is uninterested in us
            void handle_uninterested(int pfd) {
                peers.at(pfd).peer_interested = false;
            }
            
            // peer has a piece
            void handle_have(int pfd, PeerWireProtocol::Have have_resp) {
                uint64_t piece_index = have_resp.piece_index;
                
            }

            // handler function for determining the next piece to ask for in a request (should be based on bitfield/have)
            void handle_request(int pfd) {
                PeerWireProtocol::Piece piece_msg = PeerWireProtocol::Piece();
                // TODO
            }

            /**
             * Received messages as either leecher or seeder
            */

            // peer sent over a bitfield, save their bitfield
            void handle_bitfield(ProtocolUtils::Peer peer, uint8_t *peer_bitfield, uint32_t peer_bitfield_length) {
                peer.fill_bitfield(peer_bitfield, peer_bitfield_length * 8);
            }

            // let's not do these
            void handle_cancel() {}
            void handle_port() {}
    };
}
#endif