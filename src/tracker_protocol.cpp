#include "tracker_protocol.h"

namespace TrackerProtocol
{
    sockaddr_in get_tracker_addr(const std::string announce_url)
    {
        // get the host part and port of the announce url
        char *host;
        char *port;
        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURLU *handle = curl_url();
        CURLUcode rc = curl_url_set(handle, CURLUPART_URL, announce_url.c_str(), 0);
        rc = curl_url_get(handle, CURLUPART_HOST, &host, 0);
        rc = curl_url_get(handle, CURLUPART_PORT, &port, 0);

        std::string host_url(host);
        std::string port_str(port);

        // clean up stuff from curl
        curl_url_cleanup(handle);
        curl_free(host);
        curl_free(port);

        // call getaddrinfo, return the first valid ip address we find
        int status;
        addrinfo hints;
        addrinfo *servinfo;
        memset(&hints, 0, sizeof hints);

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((status = getaddrinfo(host_url.c_str(), port_str.c_str(), &hints, &servinfo)) != 0)
        {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            exit(1);
        }

        sockaddr_in tracker_addr_ret;
        sockaddr_in *tracker_addr = (sockaddr_in *)(servinfo->ai_addr);
        memcpy(&tracker_addr_ret, tracker_addr, sizeof(sockaddr_in));
        curl_global_cleanup();
        freeaddrinfo(servinfo);

        return tracker_addr_ret;
    }

    TrackerManager::TrackerManager(std::string torrent_file, std::string p_id, int c_port)
    {
        // parse metafile and obtain announce url
        std::string metainfo = Metainfo::read_metainfo_to_buffer(torrent_file);
        std::string info_dict = Metainfo::read_info_dict_str(metainfo);

        // set all fields on creation
        announce_url = Metainfo::read_announce_url(metainfo);
        info_hash = Hash::truncated_sha1_hash(info_dict, 20);
        peer_id = p_id;
        client_port = c_port;
        uploaded = "0";
        downloaded = "0";
        left = "0";
        compact = 1;
        no_peer_id = 0;
        event = EventType::STARTED;
        tracker_addr = get_tracker_addr(announce_url);

        // get socket set up
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        assert(sock >= 0);

        int connected = connect(sock, (sockaddr *)&tracker_addr, sizeof(sockaddr_in));
        assert(connected >= 0);

        sent_completed = false;
    }

    // Function to craft HTTP GET request using the provided fields
    // manually constructs the HTTP request
    // assumes all fields are defined.
    std::string TrackerManager::construct_http_string()
    {
        // get the host part of the announce URL
        CURLU *handle = curl_url();
        CURLUcode rc = curl_url_set(handle, CURLUPART_URL, announce_url.c_str(), 0);
        char *host;
        rc = curl_url_get(handle, CURLUPART_HOST, &host, 0);

        std::string host_url(host);

        // clean up
        curl_free(host);
        curl_url_cleanup(handle);

        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURL *curl = curl_easy_init();
        assert(curl);

        char *url_encoded_info = curl_easy_escape(curl, info_hash.c_str(), info_hash.length());
        char *url_encoded_peer_id = curl_easy_escape(curl, peer_id.c_str(), peer_id.length());
        assert(url_encoded_info);
        assert(url_encoded_peer_id);

        // "GET /announce?param=value&param=value HTTP/1.1\r\nHost: {}\r\n\r\n"
        std::string request = "";
        request += "GET /announce?";
        request += "info_hash=" + std::string(url_encoded_info) + "&";  // urlencoded
        request += "peer_id=" + std::string(url_encoded_peer_id) + "&"; // urlencoded
        request += "port=" + std::to_string(client_port) + "&";
        request += "uploaded=" + uploaded + "&";
        request += "downloaded=" + downloaded + "&";
        request += "left=" + left + "&";
        request += "compact=" + std::to_string(compact) + "&";
        request += "no_peer_id=" + std::to_string(no_peer_id) + "&";

        switch (event)
        {

        // must be sent first
        case STARTED:
            request += "event=started&";
            break;

        // client is shutting down gracefully
        case STOPPED:
            request += "event=stopped&";
            break;

        // client has completed the download
        case COMPLETED:
            request += "event=completed&";
            break;

        // performed at regular intervals, omit here
        case EMPTY:
            break;
        }

        // add HTTP version
        request += " HTTP/1.1\r\n";

        // add host std::string
        request += "Host: " + host_url + "\r\n\r\n";

        free(url_encoded_info);
        free(url_encoded_peer_id);
        curl_global_cleanup();

        return request;
    }

    // Pack all headers into a std::string, transmit the HTTP request with TCP
    void TrackerManager::send_http()
    {
        std::string http_req = construct_http_string();
        uint32_t length = http_req.length();
        assert(sendall(sock, http_req.c_str(), &length) == 0);
    }

    void TrackerManager::recv_http()
    {

        // Split a string on a delimiter sequence into a list of std::strings
        // this function helps parse HTTP responses into fields based on CRLF="\r\n"
        auto split = [] (std::string input, std::string delimiter)
        {
            std::vector<std::string> tokens;

            std::string token;
            size_t last_break_end = 0;
            size_t found_pos = input.find(delimiter, last_break_end);
            // find delimiter in the string, substring input between
            // last occurrenace of the delimiter and new occurrence
            // add as token
            while (found_pos != std::string::npos)
            {
                size_t substr_len = found_pos - last_break_end + 1;
                std::string token = input.substr(last_break_end, substr_len);
                tokens.push_back(token);
                last_break_end = found_pos + delimiter.length(); // need to advance past the delimiter
                found_pos = input.find(delimiter, last_break_end);
            }

            // add last token, which should run from last_break_end to end of std::string
            tokens.push_back(input.substr(last_break_end, std::string::npos));

            return tokens;
        };

        char buffer[HTTP_HEADER_MAX_SIZE];
        int response_code = 0;
        int payload_length = 0;

        // read the HTTP header, ensure that we got successful message
        // and successfully parsed payload length

        int recv_length = recv(sock, &buffer, HTTP_HEADER_MAX_SIZE, 0);
        assert(recv_length > 0); // make sure socket didn't close on us
        std::string http_resp = std::string(buffer, recv_length);

        // Split on CRLF, parse the response code and content length
        // header fields
        std::vector<std::string> lines = split(http_resp, "\r\n");
        assert(!lines.empty());

        for (auto &line : lines)
        {
            std::vector<std::string> words = split(line, " ");

            if (words[0] == "HTTP/1.1 ")
                response_code = stoi(words[1]);
            else if (words[0] == "Content-Length: ")
                payload_length = stoi(words[1]);
        }

        // make sure we got the full message, and OK status
        std::string bencoded_payload = lines.back();
        assert(response_code == HTTP_OK);
        assert(payload_length > 0);
        assert(bencoded_payload.length() == payload_length);

        // bdecode the payload and fill in fields
        parse_payload(bencoded_payload);
    }

    void TrackerManager::parse_payload(std::string payload)
    {
        bencode::data data = bencode::decode(payload);
        bencode::dict resp_dict = std::get<bencode::dict>(data);

        // If we failed, other fields are not guaranteed to exist
        if (resp_dict.find("failure reason") != resp_dict.end())
        {
            failure_reason = std::get<std::string>(resp_dict["failure reason"]);
            std::cout << failure_reason << std::endl;
        }
        else
        {

            if (resp_dict.find("tracker id") != resp_dict.end())
            {
                tracker_id = std::get<std::string>(resp_dict["tracker id"]);
            }

            interval = std::get<long long>(resp_dict["interval"]);
            num_seeders = std::get<long long>(resp_dict["complete"]);
            num_leechers = std::get<long long>(resp_dict["incomplete"]);

            // match peers onto its specified type using a visit
            std::visit([&](auto &&arg)
                       {
            using T = std::decay_t <decltype(arg)>;
            
            // Integer peers? According to bittorrent protocol, this isn't possible.
            if constexpr (std::is_same_v<T, bencode::integer>) { std::cout << "peers mapped to int?" << std::endl; }

            // Map peers? According to bittorrent protocol, this isn't possible.
            else if constexpr (std::is_same_v<T, bencode::dict>) { std::cout << "peers mapped to map?" << std::endl; }

            // Binary peer model
            else if constexpr (std::is_same_v<T, bencode::string>) {
                
                // read 6 bytes at a time for each peer 
                std::stringstream ss(arg);
                char block[6]; 
                while(ss.read(block, 6)) { 
                    
                    uint32_t ip_addr; 
                    uint16_t port; 

                    // first 4 bytes are for ip address in network byte order; 
                    // last 2 bytes are for port in network byte order;
                    memcpy(&ip_addr, block, sizeof(uint32_t));
                    memcpy(&port, block + sizeof(uint32_t), sizeof(uint16_t)); 
                    
                    Peer::PeerClient peer(ip_addr, port); 
                    peers.push_back(peer); 
                }
            }
            
            // Dictionary peer model
            else if constexpr (std::is_same_v<T, bencode::list>) {
                for(auto && entry : arg) {
                    auto peer_dict = std::get<bencode::dict>(entry); 
                    std::string peer_id = std::get<std::string>(peer_dict["peer id"]);
                    std::string ip = std::get<std::string>(peer_dict["ip"]); 
                    int port = std::get<long long>(peer_dict["port"]);
                    
                    Peer::PeerClient peer(peer_id, ip, port); 
                    peers.push_back(peer);
                }
            } }, resp_dict["peers"].base());
        }
    }
}
