#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <fcntl.h>
#include <bits/stdc++.h>
using namespace std;

#define MAX_BUFFER_SIZE 1024

unordered_map<string, string> user_db;  // username:password for every user
unordered_map<string, string> users_logged_in;  // username:receving port for only logged in users
unordered_map<string, vector<string>> group_db; // gid:list of member usernames. The first username will always be owner
unordered_map<string, unordered_set<string>> pending_join_requests; // gid:set of usernames waiting to join
unordered_map<string, unordered_set<string>> group_file_mapping;    // gid:set of files uploaded to the group
unordered_map<string, unordered_map<string, unordered_set<string>>> fileinfo;   // filename:(sha:list of ports having the file)

void client_handler(int client_socket) {
    // int client_socket = *(int *)arg;
    char buffer[MAX_BUFFER_SIZE];

    string username = "default";
    int logged_in = 0;

    // Client loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
            perror("Client disconnected");
            break;
        }

        if (strncmp(buffer, "exit", 4) == 0) {            
            break;
        }        

        string cmd(buffer);

        if (cmd.substr(0, 11) == "create_user") {
            string un_and_pwd = cmd.substr(12,cmd.size()-12);
            string un = "";
            string pwd = "";
            int space_encountered = 0;

            for(char temp:un_and_pwd){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)
                    un=un+temp;
                if(space_encountered==1)
                    pwd=pwd+temp;
            }

            cout<<"Username received is "<<un<<" and password is "<<pwd<<endl;

            if(user_db.find(un) == user_db.end()){
                string response = "Successfully registered new user";
                user_db.insert({un, pwd});
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
            else{
                string response = "Cannot register user. User already exists";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }
        else if (cmd.substr(0, 5) == "login")
        {
            string un_and_pwd_and_port = cmd.substr(6,cmd.size()-6);
            string un = "";
            string pwd = "";
            string port = "";
            int space_encountered = 0;

            for(char temp:un_and_pwd_and_port){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)
                    un=un+temp;
                if(space_encountered==1)
                    pwd=pwd+temp;
                if(space_encountered==2)
                    port=port+temp;
            }

            if(user_db.find(un) != user_db.end() && user_db[un] == pwd){
                users_logged_in[un] = port;
                username = un;
                logged_in = 1;
                string response = "User logged in successfully";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
            else{
                string response = "Invalid credentials";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }

        else if (cmd.substr(0, 12) == "create_group") {
            string gid = cmd.substr(13,cmd.size()-13);

            if(logged_in){
                if(group_db.find(gid) != group_db.end()){
                    string response = "Group already exists";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
                else{
                    group_db[gid].push_back(username);
                    cout<<"Group with group id "<<gid<<" created with owner "<<username<<endl;
                    string response = "Group created successfully";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
            }
            else{
                string response = "Please log in first";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }

        else if(cmd.substr(0, 10) == "join_group"){
            string gid = cmd.substr(11,cmd.size()-11);

            if(logged_in){
                if(group_db.find(gid) != group_db.end()){

                    vector<string> group_members = group_db[gid];
                    vector<string>::iterator ptr = find(group_members.begin(), group_members.end(), username);

                    if(ptr != group_members.end()){
                        string response = "Cannot request to be part of a group you are already a part of";
                        strcpy(buffer, response.c_str());
                        send(client_socket, buffer, sizeof(buffer), 0);
                    }
                    else{
                        pending_join_requests[gid].insert(username);

                        string response = "Join request created successfully";
                        strcpy(buffer, response.c_str());
                        send(client_socket, buffer, sizeof(buffer), 0);
                    }                    
                }
                else{
                    string response = "Group does not exist!";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
            }
            else{
                string response = "Please log in first";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }

        else if(cmd.substr(0, 14) == "accept_request"){
            string gid_and_un = cmd.substr(15,cmd.size()-15);
            string gid = "";
            string un = "";
            int space_encountered = 0;

            for(char temp:gid_and_un){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)
                    gid=gid+temp;
                if(space_encountered==1)
                    un=un+temp;
            }

            if(logged_in){
                if(group_db.find(gid) != group_db.end()){
                    
                    if(group_db[gid][0] == username){

                        // remaining: check if the userid is actually in the request set
                        if(pending_join_requests.find(gid) != pending_join_requests.end()){
                            group_db[gid].push_back(un);
                            pending_join_requests[gid].erase(un);
                            if(pending_join_requests[gid].size() == 0){
                                pending_join_requests.erase(gid);
                            }
                            string response = "Successfully accepted "+un+" into group "+gid;
                            strcpy(buffer, response.c_str());
                            send(client_socket, buffer, sizeof(buffer), 0);
                        }
                        else{
                            string response = "No pending requests for this group id";
                            strcpy(buffer, response.c_str());
                            send(client_socket, buffer, sizeof(buffer), 0);
                        }
                        
                    }
                    else{
                        string response = "You are not the owner of this group. Only owners can accept join requests";
                        strcpy(buffer, response.c_str());
                        send(client_socket, buffer, sizeof(buffer), 0);
                    }                  
                }
                else{
                    string response = "Group does not exist!";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
            }
            else{
                string response = "Please log in first";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }

        else if(cmd.substr(0, 11) == "leave_group"){
            string gid = cmd.substr(12,cmd.size()-12);

            if(logged_in){
                if(group_db.find(gid) != group_db.end()){
                    vector<string> group_members = group_db[gid];
                    vector<string>::iterator ptr = find(group_members.begin(), group_members.end(), username);

                    if(ptr == group_members.end()){
                        string response = "You are not a part of this group";
                        strcpy(buffer, response.c_str());
                        send(client_socket, buffer, sizeof(buffer), 0);
                    }
                    else{
                        pending_join_requests[gid].erase(username);

                        // delete the group itself if it has no members
                        if(pending_join_requests[gid].size() == 0){
                            pending_join_requests.erase(gid);
                        }

                        string response = "You have left group "+gid+" successfully";
                        strcpy(buffer, response.c_str());
                        send(client_socket, buffer, sizeof(buffer), 0);
                    }

                }
                else{
                    string response = "Group does not exist. Please enter valid group id";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
            }
            else{
                string response = "Please log in first";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }

        else if(cmd.substr(0, 13) == "list_requests"){
            string gid = cmd.substr(14, cmd.size()-14);

            if(logged_in){
                if(pending_join_requests.find(gid) != pending_join_requests.end()){
                    unordered_set<string> group_members = pending_join_requests[gid];
                    unordered_set<string> :: iterator itr;
                    string response = "Request list:\n";
                    for(itr = group_members.begin(); itr != group_members.end(); itr++){
                        response = response + (*itr) + "\n";
                    }

                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
                else{
                    string response = "There are no pending requests associated with this group id";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
            }
            else{
                string response = "Please log in first";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }

        else if(cmd.substr(0, 11) == "list_groups"){
            string response = "Groups list:\n";
            if(group_db.size() == 0){
                response = "No groups created yet";
            }
            else{
                for (auto i = group_db.begin(); i != group_db.end(); i++){
                    response = response + i->first + "\n";
                }
            }
            strcpy(buffer, response.c_str());
            send(client_socket, buffer, sizeof(buffer), 0);
        }

        else if(cmd.substr(0, 11) == "upload_file"){
            string fn_and_gid_and_sha = cmd.substr(12,cmd.size()-12);
            string fn = "";
            string gid = "";
            string sha = "";
            int space_encountered = 0;

            for(char temp:fn_and_gid_and_sha){
                if(temp==' ')
                {
                    space_encountered++;
                    continue;
                }
                if(space_encountered==0)
                    fn=fn+temp;
                if(space_encountered==1)
                    gid=gid+temp;
                if(space_encountered==2)
                    sha = sha + temp;
            }

            if(group_db.find(gid) != group_db.end()){
                vector<string> group_members = group_db[gid];
                vector<string>::iterator ptr = find(group_members.begin(), group_members.end(), username);

                if(ptr == group_members.end()){
                    string response = "You are not a part of this group. Please request to be part of it before uploading a file in this group";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
                else{
                    group_file_mapping[gid].insert(fn);
                    string un_port = users_logged_in[username];
                    fileinfo[fn][sha].insert(un_port);

                    string response = "File upload completed successfully";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
            }
            else{
                string response = "No such group exists. Please enter valid group id";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }
        }

        else if(cmd.substr(0, 10) == "list_files"){
            string gid = cmd.substr(11,cmd.size()-11);
            if(group_db.find(gid) != group_db.end()){
                if(group_file_mapping.find(gid) != group_file_mapping.end()){
                    unordered_set<string> files = group_file_mapping[gid];
                    unordered_set<string> :: iterator itr;
                    string response = "Files available to download:\n";
                    for(itr = files.begin(); itr != files.end(); itr++){
                        response = response + (*itr) + "\n";
                    }

                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }else{
                    string response = "No files uploaded to this group yet";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
            }
            else{
                string response = "No such group exists. Please enter valid group id";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }

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

            if(group_db.find(gid) != group_db.end()){
                vector<string> group_members = group_db[gid];
                vector<string>::iterator ptr = find(group_members.begin(), group_members.end(), username);

                if(ptr == group_members.end()){
                    string response = "You are not a part of this group";
                    strcpy(buffer, response.c_str());
                    send(client_socket, buffer, sizeof(buffer), 0);
                }
                else{
                    if(group_file_mapping[gid].find(fn) == group_file_mapping[gid].end()){
                        string response = "The file you are trying to download does not exist in this group";
                        strcpy(buffer, response.c_str());
                        send(client_socket, buffer, sizeof(buffer), 0);
                    }
                    else{
                        string response = "";
                        unordered_map<string, unordered_set<string>> innerMap = fileinfo[fn];
                        for (auto pair : innerMap) {
                            string sha = pair.first;
                            unordered_set<string> ports = pair.second;

                            // Append 'sha' to the response
                            response += sha + " ";

                            // Iterate through the set of port numbers
                            for (const string port : ports) {
                                response += port + " ";
                            }

                            // Remove the trailing ", " and add a line break
                            response.pop_back(); // Remove the trailing space
                            // response += "\n";
                        }
                        strcpy(buffer, response.c_str());
                        send(client_socket, buffer, sizeof(buffer), 0);
                    }
                }
            }
            else{
                string response = "Group does not exist. Please enter valid group id";
                strcpy(buffer, response.c_str());
                send(client_socket, buffer, sizeof(buffer), 0);
            }

        }
        
        // printf("Client: %s", buffer);
        
    }
    printf("Client exiting...\n");
    close(client_socket);
    pthread_exit(NULL);
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

        printf("Client connected. Spawning a new thread to handle it.\n");
    }   

    close(server_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Please give input in the format: ./tracker tracker_info.txt tracker_no\n");
        exit(EXIT_FAILURE);
    }

    char* filename = argv[1];
    //We need to read ip and port from the given file
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open tracker_info.txt");
        std::exit(EXIT_FAILURE);
    }

    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    if (bytesRead <= 0) {
        perror("Failed to read from tracker_info.txt");
        close(fd);
        std::exit(EXIT_FAILURE);
    }

    // Close the file descriptor
    close(fd);

    string ip, port;
    // Parse the IP and port
    buffer[bytesRead] = '\0'; // Null-terminate the buffer
    char* token = strtok(buffer, "\n");
    if (token != nullptr) {
        ip = token;
    }

    token = strtok(nullptr, "\n");
    if (token != nullptr) {
        port = token;
    }

    cout<<"My ip is "<<ip<<" and port is "<<port<<endl;

    thread server(server_thread, port);
    
    // Wait for the threads to finish
    string inp;
    cout<<"~~~> ";
    cin>>inp;
    
    if(inp == "quit"){
        exit(0);
    }
    server.join();

    return 0;
}