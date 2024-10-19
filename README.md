# Assignment 4

## Distributed File Sharing System

## Data Structures Used :
### Tracker side data structures : 

```c
unordered_map<string, string> user_db;  // username:password for every user
unordered_map<string, string> users_logged_in;  // username:receving port for only logged in users
unordered_map<string, vector<string>> group_db; // gid:list of member usernames. The first username will always be owner
unordered_map<string, unordered_set<string>> pending_join_requests; // gid:set of usernames waiting to join
unordered_map<string, unordered_set<string>> group_file_mapping;    // gid:set of files uploaded to the group
unordered_map<string, unordered_map<string, unordered_set<string>>> fileinfo;   // filename:(sha:list of ports having the file)
```

## Overview
- Tracker will store most of the data related to users, groups and files
- User will interact with the peer and peer is the interface between tracker and user
- Peer will run two threads one for serving requests from other peer and one communicating with the tracker.
- Peer will communicate to tracker and perform all tasks
- To avoid overcrowding of threads, I am spawning 10 at a time only
- I am randomly selecting which peer to download a chunk from
- For integrity check sha1 hash algorithm is used.

### Commands and their implementation :
- **Create User**
    - Peer sends request to tracker. Tracker will store the new user and allow future logins
- **Login**
    - Peer must login to run any of the further commands. Tracker will check whether client's user id and password matches or not.
- **Create Group**
    - Tracker will store the information of group created by peer and make that peer owner
- **Join Group**
    - Peer can request to join any group. 
- **Leave Group**
    - Tracker will remove user from group data structure.
- **List pending join**
    - Only owner of the group can run this command.
    - It will show a list of pending requests to a particular group
- **Accept Group Joining Request**
    - Only owner can accept the request of another peer to join group
    - Only after being accepted in a group can a peer upload to or download from the group
- **List All Group In Network**
    - Shows the list of all groups
- **List All sharable Files In Group**
    - Shows all files shared in that group
- **Upload File** 
    - Peer needs to be part of a group to upload file in that group
    - Along with file path, peer will also share sha of that file
- **Download File**
    - Peer needs to be part of a group to download files from it
    - Tracker will provide the peers which have that file, either partly or in full
    - Peer will then connect to these peers to figure out which peer has which chunk
    - It will then figure out which chunks to request and send the requests
    - The peer will then accept the chunks and write them to file in their proper place


## Execution
```shell
Tracker Terminal :
1. cd to tracker
2. g++ tracker.cpp -o tracker
3. ./tracker tracker_info.txt 1

Peer Terminal :
1. cd to client
2. g++ -o client client.cpp  -lssl -lcrypto
3. ./client 127.0.0.1:<peer_port> tracker_info.txt
```

## Assumptions 
- Tracker is always up
- Whole file will be available in the network
- Data stored on peer and tracker is not persistent. If tracker goes down, all the data will be misplaced.