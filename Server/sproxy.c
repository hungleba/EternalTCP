//Example code: A simple server side code, which echos back the received message.
//Handle multiple socket connections with select and fd_set on Linux 
#include <stdio.h> 
#include <string.h>   //strlen 
#include <stdlib.h> 
#include <errno.h> 
#include <unistd.h>   //close 
#include <arpa/inet.h>    //close 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 

#define TRUE   1 
#define FALSE  0

struct Packet {
	int seq;
	int length;
	char header[20];
	char buffer[1025];
	struct Packet* next;
};

struct Packet* dequeue(struct Packet* head, int threshhold) {
	// dequeue all packet < threshhold
	struct Packet* curr = head;
	while (curr) {
		if (curr->seq >= threshhold) {
			return curr;
		}
		curr = curr->next;
	}
	return curr;
}

struct Packet* addQueue(struct Packet* head, int new_seq, int new_length, char* new_header, char* new_buffer) {
	// Create new node
	struct Packet* new_node= malloc(sizeof(struct Packet));
	new_node->seq = new_seq;
	new_node->length = new_length;
	memcpy(new_node->header, new_header, 20);
	memcpy(new_node->buffer, new_buffer, new_length);
	new_node->next = NULL;
	// Add to the back of the queue
	struct Packet* curr = head;
	if (!curr) {
		return new_node;
	} else {
		while (curr) {
			if (curr->next == NULL) {
				curr->next = new_node;
				return head;
			}
			curr = curr->next;
		}
	}
	return head;
}

void fillHeader(char *header, int isHeartbeat, int curr_session_id, int curr_seqN, int curr_ackN, int length) {
	int *headerInt = (int *) header;
	*headerInt = isHeartbeat;
	headerInt++;

	*headerInt = curr_session_id;
	headerInt++;

	*headerInt = curr_seqN;
	headerInt++;

	*headerInt = curr_ackN;
	headerInt++;

	*headerInt = length;
	headerInt++;
}

void sendHeartbeat(int curr_session_id, int curr_seqN, int curr_ackN, int socket) {
	char header[20];
	// isHeartbeat = 1 and length = 0
	fillHeader(header, 1, curr_session_id, curr_seqN, curr_ackN, 0);
	send(socket, header, 20, 0);
	bzero(header, sizeof(header));
}

void resendPackets(struct Packet *head, int socket) {
	struct Packet *curr = head;
	int i = 0;
	while (curr) {
		send(socket, curr->header, 20, 0);
		send(socket, curr->buffer, curr->length, 0);
		curr = curr->next;
		i++;
	}
}

int main(int argc , char *argv[])  
{  
	int opt = TRUE;  
	int master_socket , addrlen , client_socket, 
	    server_socket , activity, valread;
	struct timeval last_hb_sent;
	struct timeval last_hb_recv;
	struct timeval curr_time;
	int max_sd = 0, curr_session_id = 0;
	int curr_seqN = 0, curr_ackN = 0;
	struct Packet* head = NULL;
	struct sockaddr_in address;
	struct sockaddr_in servaddr;

	char buffer[1025];  //data buffer of 1K 
	char header[20];	//5 integers 

	//set of socket descriptors 
	fd_set readfds;

	//initialise all sockets to 0 so not checked 
	client_socket = 0;
	server_socket = 0;

	//create a master socket 
	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)  
	{  
		perror("socket failed");  
		exit(EXIT_FAILURE);  
	}  else 
		printf("Master socket fd is: %d\n", master_socket); 

	//set master socket to allow multiple connections , 
	//this is just a good habit, it will work without this 
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, 
				sizeof(opt)) < 0 )  
	{  
		perror("setsockopt");  
		exit(EXIT_FAILURE);  
	}  

	//type of socket created 
	address.sin_family = AF_INET;  
	address.sin_addr.s_addr = INADDR_ANY;  
	address.sin_port = htons( atoi(argv[1]) );  

	//configure server address
	servaddr.sin_family = AF_INET;  
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons( 23 );

	//bind the socket to localhost port 8888 
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)  
	{  
		perror("bind failed");  
		exit(EXIT_FAILURE);  
	}  
	printf("Listener on port %d \n", atoi(argv[1]));  

	//try to specify maximum of 3 pending connections for the master socket 
	if (listen(master_socket, 3) < 0)  
	{  
		perror("listen");  
		exit(EXIT_FAILURE);  
	}  

	//accept the incoming connection 
	addrlen = sizeof(address);  
	puts("Waiting for connections ...");
	int flag = 0;
	while(TRUE) {  
		//clear the socket set 
		FD_ZERO(&readfds);
		//FD_ZERO(&writefds);
		if (flag <= 0) {
			if (client_socket <= 0) {
				if ((client_socket = accept(master_socket, 
								(struct sockaddr *)&address, (socklen_t*)&addrlen))<0)  {  
					perror("accept");  
					exit(EXIT_FAILURE); 
				} else {
					printf("New connection from cproxy\n");
					gettimeofday(&last_hb_recv, NULL);
				}
			}
			//add new socket to array of sockets 
			//create server socket
			if (server_socket <= 0) {
				server_socket = socket(AF_INET, SOCK_STREAM, 0);
				if (server_socket == -1) {
					printf("socket creation failed...\n");
					exit(0);
				} else
					printf("Socket successfully created, fd: %d\n", server_socket);

				//connect to the server
				if (connect(server_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
					printf("connection with the server failed...\n");
					close(server_socket);
					server_socket = -1;
				} else {
					printf("connected to the server..\n");
				}
			}
			if (client_socket > 0 && server_socket > 0) flag = 1;
		}

		if (flag <= 0) continue;
		//add child sockets to set 
		if (client_socket > 0) {
			FD_SET(client_socket, &readfds);
			if (client_socket > max_sd)
				max_sd = client_socket;
		}

		if (server_socket > 0) {
			FD_SET(server_socket, &readfds);
			if (server_socket > max_sd)
				max_sd = server_socket;
		}
		//wait for an activity on one of the sockets , timeout is NULL , 
		//so wait indefinitely 
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		activity = select( max_sd + 1 , &readfds , NULL , NULL , &tv);  

		if ((activity < 0) && (errno!=EINTR)) {  
			printf("select error");
		}
		
		gettimeofday(&curr_time, NULL);
		// long int ti = (curr_time.tv_sec - last_hb_sent.tv_sec);
		// if (ti >= 1) {
		// 	sendHeartbeat(curr_session_id, curr_seqN, curr_ackN, client_socket);
		//	gettimeofday(&last_hb_sent, NULL);
		// }
		long int ti = (curr_time.tv_sec - last_hb_recv.tv_sec);
		if (ti >= 3) {
			printf("Disconnecting from cproxy...\n");
			close(client_socket);
			client_socket = 0;
			flag = 0;
		}
		//If something happened on the master socket , 
		//else its some IO operation on client socket

		if (FD_ISSET(client_socket , &readfds)) { 
			//Check if it was for closing , and also read the 
			//incoming message 
			if ((valread = read( client_socket , header, 20)) == 0) {  
				//Somebody disconnected , get his details and print 
				getpeername(client_socket , (struct sockaddr*)&address , \
						(socklen_t*)&addrlen);  
				printf("Host disconnected , ip %s , port %d \n" , 
						inet_ntoa(address.sin_addr) , ntohs(address.sin_port));  

				//Close the socket and mark as 0 in list for reuse
				close( server_socket );
				close( client_socket );  
				server_socket = 0;
				client_socket = 0;
				flag = 0;
			}  

			//Echo the message to the server
			else {
				// decode packet
				int* headerInt = (int*) header;
				int isHeartbeat = *headerInt;
				headerInt++;

				int session_id = *headerInt;
				headerInt++;

				int seqN = *headerInt;
				headerInt++;

				int ackN = *headerInt;
				headerInt++;

				int length = *headerInt;
				headerInt++;

				if (isHeartbeat) {
					gettimeofday(&last_hb_recv, NULL);
					printf("Heartbeat received ");
					if (session_id == curr_session_id) { 
						printf("(same session)\n");
					} else {
						printf("(Alert: different session)\n");
						curr_session_id = session_id;
						// Reset all data
						head = NULL;
						curr_seqN = 0;
						curr_ackN = 0;
						// Disconnect from server_socket
						close(server_socket);
						server_socket = 0;
						flag = 0;
					}
					sendHeartbeat(curr_session_id, curr_seqN, curr_ackN, client_socket);
					gettimeofday(&last_hb_sent, NULL);
				} else {
					// read message based on length
					valread = read(client_socket, buffer, length);

					// Receive package only if all previous packages were delivered
					if (curr_ackN == seqN) {
						// Send message to telnet
						send(server_socket , buffer , length, 0);
						curr_ackN++;
					}
				}
				head = dequeue(head, ackN);
				// Re-send all missing packages
				resendPackets(head, client_socket);
				
				bzero(header, sizeof(header));
				bzero(buffer, sizeof(buffer));
			}  
		}  

		//else its some IO operation on server socket
		if (FD_ISSET( server_socket , &readfds))  {
			//Check if it was for closing , and also read the 
			//incoming message 
			if ((valread = read( server_socket , buffer, 1025)) <= 0)  {  
				//Somebody disconnected , get his details and print 
				getpeername(server_socket , (struct sockaddr*)&address , \
						(socklen_t*)&addrlen);  
				printf("Host disconnected , ip %s , port %d \n" , 
						inet_ntoa(address.sin_addr) , ntohs(address.sin_port));  

				//Close the socket and mark as 0 in list for reuse
				close( server_socket );
				close( client_socket );  
				server_socket = 0;
				client_socket = 0;
				flag = 0;
			} else {
				// Send header first
				char headerToSend[20];
				fillHeader(headerToSend, 0, curr_session_id, curr_seqN, curr_ackN, valread);
				send(client_socket, headerToSend, 20, 0);
				// Send payload later
				send(client_socket , buffer , valread , 0);
				// Save to the queue, increase seq
				head = addQueue(head, curr_seqN, valread, headerToSend, buffer);
				curr_seqN++;
				bzero(headerToSend, sizeof(headerToSend));
				bzero(buffer, sizeof(buffer));
			}  
		} 
	}  

	return 0;  
}  
