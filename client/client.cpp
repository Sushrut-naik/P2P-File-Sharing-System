#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <fcntl.h>
#include<bits/stdc++.h>
#include <sys/stat.h>
#include <cmath>
#include <semaphore.h>
#include <sstream>
#include <openssl/sha.h>
#include <mutex>
#include <condition_variable>

const int maxConcurrentThreads = 10;

std::mutex mtx;

using namespace std;
sem_t semaphore;
#define MAX_BUFFER_SIZE 32768
unordered_map<string, int> file_to_number_of_chunks_mapping;

string calculateSHA1(const string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA_CTX sha1;
    SHA1_Init(&sha1);
    SHA1_Update(&sha1, input.c_str(), input.size());
    SHA1_Final(hash, &sha1);

    char hexHash[2 * SHA_DIGEST_LENGTH + 1];
    hexHash[2 * SHA_DIGEST_LENGTH] = 0;

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(&hexHash[i * 2], "%02x", hash[i]);
    }

    return string(hexHash);
}

string get_sha ( string filePath )
{
    cout << "inside gethasoffile fucntion " << endl ;
    int tempfd = open(filePath.c_str() , O_RDONLY ) ;

    SHA_CTX shaContext ;
    SHA1_Init(&shaContext);

    int bufferSize = 1024 ;
    char buffer[bufferSize];
    int bytesRead = 0 ; 

    while ( (bytesRead = read (tempfd , buffer , bufferSize)) >0  )
    {
        SHA1_Update(&shaContext , buffer ,bytesRead);
    }

    close (tempfd);
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final ( hash , &shaContext);

    char hashString[ 2* SHA_DIGEST_LENGTH + 1];

    for ( int i = 0 ; i < SHA_DIGEST_LENGTH ; i++ )
    {
        sprintf(&hashString[i * 2], "%02x", hash[i]);
    }

    hashString[ 2* SHA_DIGEST_LENGTH ] = '\0' ;

    cout << "hashof file is : " << hashString << endl ;
    return hashString  ;

}

void client_handler(int client_socket) {
    // int client_socket = *(int *)arg;
    char buffer[MAX_BUFFER_SIZE];
    string chunk_to_send;
    ssize_t bytes_read;
    // Client loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
            perror("Client disconnected");
            break;
        }

        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Client exiting...\n");
            break;
        }

        string cmd(buffer);
        

        if (cmd.substr(0, 6) == "i want") {
            string un_and_pwd = cmd.substr(7,cmd.size()-7);
            string fn = "";
            int space_encountered = 0;

            for(char temp:un_and_pwd){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)
                    fn=fn+temp;
            }

            cout<<"Received filename "<<fn<<endl;

            // got the filename. check if it is present in file_to_chunk. send all chunks if it is
            if(file_to_number_of_chunks_mapping.find(fn) != file_to_number_of_chunks_mapping.end()){
                // i have the entire file
                string response = "";

                for(int i=0;i<file_to_number_of_chunks_mapping[fn];i++){
                    response = response + to_string(i) + " ";
                }

                // remove extra whitespace
                response.pop_back();
                cout<<"Response string is "<<response<<endl;

                cout<<"Sending the chunks\n";

                // strcpy(buffer, response.c_str());
                send(client_socket, response.c_str(), response.length(), 0);

            }

        }
        else if(cmd.substr(0, 11) == "give chunks"){
            cout<<"Inside give chunks\n";
            string fn_and_cn = cmd.substr(12,cmd.size()-12);
            string fn = "";
            string cn = "";
            int space_encountered = 0;

            for(char temp:fn_and_cn){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)
                    fn=fn+temp;
                if(space_encountered==1)
                    cn=cn+temp;
            }

            // read the chunk from the file and calculate sha for that chunk
            size_t chunkSize = MAX_BUFFER_SIZE;

            // Convert cn from string to integer
            int chunkNumber = std::stoi(cn);

            // Open the file using the open system call
            int fileDescriptor = open(fn.c_str(), O_RDONLY);
            if (fileDescriptor == -1) {
                cout<<"Failed to open the file\n";
                return;
            }

            // Seek to the position of the chunk
            off_t chunkOffset = static_cast<off_t>(chunkNumber) * static_cast<off_t>(chunkSize);
            if (lseek(fileDescriptor, chunkOffset, SEEK_SET) == -1) {
                close(fileDescriptor);
                cout<<"Failed to seek to chunk position\n";
                return;
            }

            // Read the chunk
            char buffer[chunkSize];
            ssize_t bytesRead = read(fileDescriptor, buffer, chunkSize);
            close(fileDescriptor);
            cout<<"Number of bytes read = "<<bytesRead<<endl;
            bytes_read = bytesRead;

            if (bytesRead == -1) {
                cout<<"Failed to read the chunk\n";
                return;
            }

            string read_chunk(buffer, bytesRead);
            cout<<"Read chunk from file on server side\n";
            // cout<<"HERE read chunk "<<read_chunk<<endl;
            chunk_to_send = read_chunk;

            string sha_to_send = calculateSHA1(read_chunk);
            // string final = sha_to_send + ";;" + read

            cout<<"Now sending sha to peer "<<sha_to_send<<endl;

            strcpy(buffer, read_chunk.c_str());
            send(client_socket, &read_chunk[0], read_chunk.length(), 0);

            string send_bytes = to_string(bytesRead);
            // send_bytes = send_bytes+"\0";
            // send(client_socket, send_bytes.c_str(), send_bytes.length(), 0);
        }

        else if(cmd.substr(0, 12) == "sha received"){

            cout<<"Sending chunk "<<chunk_to_send<<endl;
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, chunk_to_send.c_str());
            cout<<"Size of buffer is "<<sizeof(buffer)<<endl;
            cout<<"Buffer is "<<buffer<<endl;
            send(client_socket, chunk_to_send.c_str(), chunk_to_send.length(), 0);
        }
    }

    cout<<"Closing connection to peer\n";
    close(client_socket);
}

void server_thread(string port) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[MAX_BUFFER_SIZE];

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Server socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(stoi(port));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    pthread_t spawn_thread;

    // Bind the server socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Server socket bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Server socket listen failed");
        exit(EXIT_FAILURE);
    }

    // printf("Server listening on port %s...\n", (char *)port);
    cout<<"Server listening on port "<<port<<endl;


    //Now, we want to spawn a new thread for every client connection that comes in
    while (1) {
        // Accept client connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Client socket accept failed");
            exit(EXIT_FAILURE);
        }

        // Create a new thread to handle the client
        thread(client_handler, client_socket).detach();

        printf("Peer connected. Spawning a new thread to handle it.\n");
    }   

    close(server_socket);
}

void client_thread(string port, string chunkno, string filename, string dp) {
    cout<<"Inside client thread"<<endl;
    fflush(stdout);
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[MAX_BUFFER_SIZE];

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Client socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    // server_addr.sin_port = htons(atoi((char *)arg));
    server_addr.sin_port = htons(stoi(port));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Client connection failed");
        exit(EXIT_FAILURE);
    }

    // asking the client for the required chunk

    memset(buffer, 0, sizeof(buffer));

    cout<<"Requesting chunks from the peer\n";

    string request = "give chunks " + filename + " " + chunkno;
    send(client_socket, request.c_str(), request.length(), 0);

    cout<<"Getting the sha\n";
    // accept sha before accepting chunk
    long long int bytes_rec;
    if ((bytes_rec = read(client_socket, buffer, sizeof(buffer))) <= 0) {
        perror("Peer disconnected");
    }

    cout<<"SHA received from peer\n";
    string chunk(buffer, bytes_rec);
    cout<<"Bytes received are "<<bytes_rec<<endl;
    cout<<"Size of chunk received is "<<chunk.length();

    // accept the incoming bytes
    
    // if (bytes_rec = read(client_socket, buffer, sizeof(buffer), 0) <= 0) {
    //     perror("Peer disconnected");
    // }
    // cout<<"Bytes read for expected size are "<<bytes_rec<<endl;
    // string chunk_length(buffer, bytes_rec);

    // long long int exp_bytes = stoi(chunk_length);

    // string sha_rec = "sha received";

    // send(client_socket, sha_rec.c_str(), sha_rec.length(), 0);

    //  Now accept chunk
    // char* temp = new char[MAX_BUFFER_SIZE];

    // Receive the chunk data
    // if (recv(client_socket, temp, MAX_BUFFER_SIZE, 0) <= 0) {
    //     perror("Peer disconnected");
    //     delete[] temp;
    //     return;
    // }
    // cout<<"Value of temp is "<<temp<<endl;
    // // Create a string from the received data
    // string chunk(temp);
    // delete[] temp;


    // long long int received_until_now = 0;
    // string res_buf = "";

    // while(1){
    //     bzero(buffer, sizeof(buffer));
    //     bytes_rec = read(client_socket, buffer, sizeof(buffer));
    //     cout<<"Bytes received in loop "<<bytes_rec<<endl;
    //     received_until_now += bytes_rec;
    //     string result(buffer);
    //     res_buf += result;

    //     if(bytes_rec == 0){
    //         break;
    //     }

    // }

    // cout<<"Received chunk from peer\n";
    
    // string chunk = res_buf;
    // cout<<"Received chunk is "<<chunk<<endl;
    // string calculated_sha = calculateSHA1(chunk);

    // // calculate sha for received chunk and verify if it matches with chunk_sha. Only write to file if sha matches

    // cout<<"Received sha is "<<chunk_sha<<endl;
    // cout<<"Calculated sha is "<<calculated_sha<<endl;

    // if(chunk_sha == calculated_sha){
        cout<<"Chunk verified. Writing to file\n";

        string full_path = dp+filename;
        int fileDescriptor = open(full_path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (fileDescriptor == -1) {
            // Failed to open the file
            std::cerr << "Failed to create the file: " << full_path << std::endl;
        } else {
            // Use lseek to set the file offset to the desired position
            sem_wait(&semaphore);
            off_t offset = stoi(chunkno) * MAX_BUFFER_SIZE;
            cout<<"Offset for chunk number "<<chunkno<<" is "<<offset<<endl;
            off_t result = lseek(fileDescriptor, offset, SEEK_SET);

            if (result == -1) {
                // Failed to seek to the desired position
                cerr << "Failed to seek to the desired position" << endl;
            } else {
                // File offset is now set to the desired position
                const char* data = "Hello, this is a sample text.\n";
                int dataSize = strlen(data);

                // Write data to the file
                ssize_t bytesWritten = write(fileDescriptor, chunk.c_str(), chunk.length());

                if (bytesWritten == -1) {
                    // Failed to write to the file
                    cerr << "Failed to write to the file" << endl;
                } else {
                    cout << "File created and data written successfully: " << full_path << endl;
                }
            }
            sem_post(&semaphore);
            // Close the file descriptor when you're done with it
            close(fileDescriptor);
        }
    // }

    if (strncmp(buffer, "exit", 4) == 0) {
        printf("Client exiting...\n");       
    }

    close(client_socket);
    // std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./client <IP>:<PORT> tracker_info.txt\n");
        exit(EXIT_FAILURE);
    }    

    if (sem_init(&semaphore, 0, 1) != 0) {
        cerr << "Semaphore initialization failed" << endl;
        return EXIT_FAILURE;
    }

    string my_ip;
    string my_port;
    string tracker_ip;
    string tracker_port;

    string ip_port_str = argv[1];
    size_t colon_pos = ip_port_str.find(":");
    if (colon_pos != string::npos) {
        my_ip = ip_port_str.substr(0, colon_pos);
        my_port = ip_port_str.substr(colon_pos + 1);
    } else {
        perror("Invalid format for <IP>:<PORT>");
        exit(EXIT_FAILURE);
    }

    thread server(server_thread, my_port);

    char* filename = argv[2];
    //We need to read ip and port from the given file
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open tracker_info.txt");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    if (bytesRead <= 0) {
        perror("Failed to read from tracker_info.txt");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close the file descriptor
    close(fd);

    buffer[bytesRead] = '\0'; // Null-terminate the buffer
    char* token = strtok(buffer, "\n");
    if (token != nullptr) {
        tracker_ip = token;
    }

    token = strtok(nullptr, "\n");
    if (token != nullptr) {
        tracker_port = token;
    }

    //Now that we have the tracker port and ip, we connect to the tracker and let it know that we are alive
    struct sockaddr_in server_addr, peer_addr;
    // char buffer[MAX_BUFFER_SIZE];

    int client_socket;
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Client socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(stoi(tracker_port));
    // server_addr.sin_port = htons(9901);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    cout<<"Connecting to tracker port "<<tracker_port<<endl;

    // Connect to the tracker
    int tracker_fd;
    if (tracker_fd = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Client connection failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        cout<<"~~~~> ";
        string cmd;
        memset(buffer, 0, sizeof(buffer));
        printf("Enter a message: ");
        getline(cin, cmd);
        if (cmd == "exit") {
            printf("Client exiting...\n");
            break;
        }        

        if (cmd.substr(0, 11) == "create_user") {
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }
        
        else if (cmd.substr(0, 5) == "login")
        {
            cmd = cmd + " " + my_port;
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if (cmd.substr(0, 12) == "create_group")
        {
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 10) == "join_group"){
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 14) == "accept_request"){
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 11) == "leave_group"){
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 13) == "list_requests"){
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 11) == "list_groups"){
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 11) == "upload_file"){

            string fn_and_gid = cmd.substr(12,cmd.size()-12);
            string fn = "";
            string gid = "";
            int space_encountered = 0;

            for(char temp:fn_and_gid){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)
                    fn=fn+temp;
                if(space_encountered==1)
                    gid=gid+temp;
            }

            string sha = get_sha(fn);
            string send_sha = sha.substr(0, 20);
            cmd = cmd + " " + send_sha;

            struct stat fileStat;

            // calculating number of chunks using file size
            off_t fsize;
            if (stat(fn.c_str(), &fileStat) == 0) {
                // fileStat.st_size contains the file size in bytes
                fsize =  fileStat.st_size;
            } else {
                std::cerr << "Failed to get file size." << std::endl;
            }

            // chunk size is 512kB = 512*1024 bytes

            // int num_of_chunks = ceil(static_cast<double>(fsize) / (512 * 1024));
            int num_of_chunks = ceil(static_cast<double>(fsize) / (MAX_BUFFER_SIZE));
            cout<<"Number of chunks is "<<num_of_chunks<<endl;

            file_to_number_of_chunks_mapping[fn] = num_of_chunks;

            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 10) == "list_files"){
            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);
            cout<<response<<endl;
        }

        else if(cmd.substr(0, 13) == "download_file"){
            string gid_and_fn_and_dp = cmd.substr(14,cmd.size()-14);
            string fn = "";
            string gid = "";
            string dp = "";
            int space_encountered = 0;

            for(char temp:gid_and_fn_and_dp){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)                    
                    gid=gid+temp;
                if(space_encountered==1)
                    fn=fn+temp;
                if(space_encountered==2)
                    dp = dp + temp;
            }

            send(client_socket, cmd.c_str(), cmd.length(), 0);
            if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                perror("Client disconnected");
                break;
            }

            string response(buffer);

            // a local map here which will contain (chunk_num : list of ports that have it)
            // index the chunks from 0 as position to write the chunk will then be chunk_num*chunk_size

            unordered_map<string, vector<string>> chunk_port_mapping;
            string full_file_sha = response.substr(0, 20);
            string list_of_ports = response.substr(21,response.size()-21);

            cout<<"Full response is "<<response<<endl;
            cout<<"full sha is "<<full_file_sha<<endl;
            cout<<"List of ports is "<<list_of_ports<<endl;

            vector<string> port_vector;

            istringstream iss(list_of_ports);
            string port;

            while (iss >> port) {
                cout<<"Pushing port "<<port<<endl;
                port_vector.push_back(port);
            }

            string returned_ports;

            // connect to each peer and receive which chunks they have
            for(auto curr_port : port_vector){
                int temp_socket;
                // Create socket
                temp_socket = socket(AF_INET, SOCK_STREAM, 0);
                if (temp_socket == -1) {
                    perror("Peer socket creation failed");
                    exit(EXIT_FAILURE);
                }

                // Configure server address
                memset(&peer_addr, 0, sizeof(peer_addr));
                peer_addr.sin_family = AF_INET;
                peer_addr.sin_port = htons(stoi(curr_port));
                // server_addr.sin_port = htons(9901);
                peer_addr.sin_addr.s_addr = INADDR_ANY;
                cout<<"Connecting to peer port "<<curr_port<<endl;

                int temp_fd;
                if (temp_fd = connect(temp_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) == -1) {
                    perror("Client connection failed");
                    exit(EXIT_FAILURE);
                }

                string request = "i want " + fn;
                send(temp_socket, request.c_str(), request.length(), 0);
                char b[1000000];
                if (recv(temp_socket, b, sizeof(b), 0) <= 0) {
                    perror("Peer disconnected");
                    break;
                }

                string peer_response(b);
                cout<<"Peer response is "<<peer_response<<endl; 

                request = "exit";
                send(temp_socket, request.c_str(), request.length(), 0);

                vector<string> chunk_vector;

                istringstream iss(peer_response);
                string chunk;

                while (iss >> chunk) {
                    cout<<"Pushing chunk "<<chunk<<endl;
                    chunk_vector.push_back(chunk);
                }

                for(auto ch:chunk_vector){
                    chunk_port_mapping[ch].push_back(curr_port);
                }
            }

            // Now that you have the mapping, select which chunk you want from which client
            vector<std::thread> threadPool;
            for(auto pair: chunk_port_mapping){
                string chunkno = pair.first;
                vector<string> vec = pair.second;

                int idx = rand() % vec.size();
                cout<<"Will be requesting chunk number "<<chunkno<<" from port "<<vec[idx]<<endl;

                // now pass this chunkno and the port you want it from(vec[idx]) to client_thread function 
                // assuming that the thread will directly write the chunk into file and return

                threadPool.emplace_back(client_thread, vec[idx], chunkno, fn, dp);

                if (threadPool.size() >= maxConcurrentThreads) {
                    // Wait for the current batch of threads to finish
                    for (std::thread& t : threadPool) {
                        t.join();
                    }
                    // Clear the thread pool
                    threadPool.clear();
                }
                // try{
                //     thread client(client_thread, vec[idx], chunkno, fn, dp);
                //     client.detach();
                //     // client_thread(vec[idx], chunkno, fn, dp);
                // }
                // catch(const std::exception& e){
                //     std::cerr << "Exception in client_thread: " << e.what() << std::endl;
                // }
                
            }
            for (std::thread& t : threadPool) {
                t.join();
            }

        }

        
    }



    // Create a client thread
    // thread client(client_thread, argv[1]);

    // Wait for the threads to finish
    server.join();
    // client.join();

    return 0;
}