# BitTorrent2

## File Structure
- ```include``` folder holds all header files (.hpp)
- ```lib``` folder holds all external libraries
- ```src``` folder holds all source code (.cpp)
- ```test``` folder for random testing

## Usage
```
mkdir build 
cd build 
cmake .. 
cmake --build . 
```
## TODO 
- Goal for today: download pieces 
    - x Bitfield working 
        - x Save bitfield of each client after handshake if given
        - May need to update bitfield after receiving have
    - Request working 
        - x Analyze bitfield to see what first piece we dont have to request
            - Within the piece, we need to see what block to request (probably some greedy search)
    - Piece working
        - After Request should receive piece then must save that to data structure (bytestream)

## Protocol 
- Client reads torrent file -> Client sends HTTP GET to tracker -> tracker responses with peers 
    - Talk to each peer
        - Handshake
        - if we have pieces of the file, send bitfield 
        - send interested, wait for unchoke
        - if unchoked, 
    - Every ~interval seconds (can do less) send a HTTP GET to tracker again with updates
        - tracker responds with updated list of peers


- Client needs to: 
    - Maintain list of peers for each torrent file
        - Each peer needs to maintain active status: whether or not this peer is still active in a given torrent
            - For leeching: if the peer doesn't talk for 2 minutes, we close the connection
            - For seeding: if the peer doesn't talk for 2 minutes, we close the connection

        - Want: fast indexing to the right peer, and deletes will be frequent. Map?

#### Threads
- Timeout Thread
    - Waits for set amount of time (120 seconds)  
    - (SEEDER) Loops through list of clients we are handling as a seeder checks to see if timeout flag is true or not which indicates whether or not they sent a message within the 2 minute timeframe
    - (LEECHER) Loops through list of peers we are requesting from as leecher and sends a keep alive for any that have not sent a message within that time 
        - Possbily run this a little bit earlier to prevent timeouts occuring for leecher requests
        - 110 seconds

- Torrenting thread: Leecher/Seeder Thread
    - Responsible for handling all messages between clients - clients 
    - Should update bitfield or have message as leecher finds new data
    
- Tracker Server Thread
    - Responsible for sending Tracker requests/handling tracker responses to the tracker server at the interval specified by the tracker server
    - Should repeat for interval time specified by server
    - when seeding stops, also send a final message to tracker

- Sockets: 
    - create socket for talking to tracker, socket(), connect()
    - connect() to tracker
    - connect()s to peers (peers list given from tracker) <-- LEECH HERE (does the seeder need to connect if they already can talk to the client?)
    - listen() socket for incoming connections <-- SEED HERE

## Roles
- Richard: bencoding, tracker, leecher
- Jason: leecher, self-host tracker
- Ryan: seeder, unit tests
- Alex: unit tests, timeout

## Classes

Torrent
- Peer(s)
- SingleFileInfo
    - Bitfield
    - FilePiece(s)
        - Bitfield
- Message(s)
- Tracker

Bitfield:
- create/modify/check bitfield inside

FilePiece: contains information about a piece
- has a Bitfield for its blocks
- check if piece (itself) is fully downloaded into the buffer (check block bitfield and verify hash) -> return pointer to output_buffer
- write piece (itself) to disk
- request piece (itself) from a peer (send request msg for every block in the piece (MAY OR MAY NOT BE IN THIS CLASS))
- given a block, "download" it to the piece buffer

Peer: contains peer states and information
- has an (initially empty) Bitfield
- given a bitfield, fill in peer's (itself) bitfield
- set the peer (itself) as offline

Torrent: contains information about the torrent
- has a Bitfield for its pieces
- send to tracker
- receive from tracker
- send to peers
    - helper: send request for a piece -> send BATCH of requests for all blocks in that piece
- receive from peers
- check if torrent (itself) is fully downloaded (check piece Bitfield)

## day of reckoning
high prio
- block queue in tandem with block bitfield  ============ CURRENTLY WORKING ON =================
    - queue for request msgs to send with the requested block
    - when requesting a piece, deq every request msg off the queue to peer
    - when receiving pieces, mark off received bits in the bitfield
    - once every request msg is sent, perform O(n) sweep on the bitfield to determine blocks that need requests resent
    - queue the missing block requests and repeat

- block queue in tandem with block bitfield
    - queue request msgs for a single piece
    - send requests in queue
    - after receiving the last block:
        - mark off received bits in block bitfield
        - requeue the blocks that need to be requested again
        - 

- seeding


low prio
- change all occurrences of char pointers to vectors
- make commandline options (do seeder vs leecher mode)
