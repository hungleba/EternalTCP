# EternalTCP
Keeps your TCP connection live anywhere anytime
# Problem
When a client moves to a new network, it will change its IP address. This will break all
Existing TCP connections. All traffic from the client to the server will carry the new address,
but the server cannot identify it with any existing TCP connection, so it will just drop the traffic
# How to avoid this?
1. Detect that existing connections don’t work any more.
2. Initiate new connections, which will be established by the new address.
3. Remember the application states (e.g., partially downloaded file), so that it
can resume from where it was left.

When the client address changes, it doesn’t affect telnet or telnet daemon since they both connect to localhost.
Only the cproxy-sproxy connection will be affected. But we can enhance them so that they can detect the
connection loss, reconnect, and resume from where it stopped
![image](https://user-images.githubusercontent.com/51266998/183240914-4e0535f9-9d70-4551-951f-9fb22631a9fe.png)
# Design
### Design for packet format:
We will have 2 main part of the packet which are the header and payload.
With the packet header (20 bytes), we will stored 5 integers:
1. **is_heartbeat**: to notify whether this packet is a heartbeat or not. (is_heartbeat is 1
means that the packet is heartbeat and 0 mean it is not).
2. **curr_sessionID**: current session ID. This will be used to check whether telnet made
new connection to cproxy or not. (If the curr_session of cproxy is different from the sproxy, then
it means that a new connection has been made). Each time the telnet make a new connection to
cproxy then the curr_sessionID will be increased by 1.
3. **curr_seqN**: This will be used to notify how many packets had been sent from that
proxy.
4. **curr_ackN**: This will be used to notify how many packets are received in by that proxy.
5. **length**: the length of the buffer / the length of the message will be sent
### Design for a queue in cproxy and sproxy to store the packet for reliable transfer:
There will be a queue to store the transmitted packet, which will be
used to keep track and resend packet to avoid packet loss.

The queue will be implemented as a linked-list with each node is each packet that had been sent.
Within each packet (node), there will be 4 fields inside: header (this is also the header of the
packet), the buffer / message, the seqN, and the length of buffer. With those information, the
sproxy and cproxy can keep track of the amount of packet that had been sent / received and also
what is inside those packets.

When the proxy sends a packet, it will add the packet to the queue with all the data mentioned
above.

When a packet is coming, the proxy will only receive that packet if the curr_ackN is equal to the
seqN of that packet.
# Install
You will need two middle servers to ensure the connection is stable. A server will host **cproxy** and another host **sproxy**.
1. Start the sproxy at port 6200
2. Start the cproxy at port 5200 then make a TCP connection to sproxy at port 6200
3. Connect telnet localhost to cproxy at 5200
4. Connect sproxy to your desired server
# What's next?
After this point, we will have connections from telnet ⇌ cproxy, cproxy ⇌ sproxy,
sproxy ⇌ telnet daemon. Telnet daemon will prompt a new login to the telnet and after logging in,
they will start exchanging packet. Cproxy will send a heartbeat to Sproxy to check if that one is still
alive, if the sproxy receives the heartbeat then it will send a heartbeat back to Cproxy (Cproxy will do
the same thing when receives the heartbeat). The heartbeat will be sent every second.
