Team Members:-
Tapti Palit [109293381]
Kavita Agarwal [109366872]

Key points:-

1) To ensure that only unicast addresses of the client interface are bound, we consider the primary address as returned by (ifi_addr) field of
the ifi_info structure. We do not take into account the broadcast address(ifi_brdaddr) while binding the client.

2) We have taken time on a milliseconds granularity considering the kind of LAN environment (the compservs) are on.
The float fields of the rttinfo struct have been changed to integer fields so as to avoid floating point arithmetic
and use shift operators for faster integer calculations.

For milliseconds granularity:-
RTT_RXTMIN   to 1000 msec
RTT_RXTMAX  to 3000 msec
RTT_MAXNREXMT  to 12

rtt_stop was modified to compute the new rto using following integer calculations:-

measuredRTT –= (srtt >> 3);
srtt += measuredRTT;
if ( measuredRTT < 0 )
measuredRTT = –measuredRTT;
measuredRTT –= (rttvar >> 2);
rttvar += measuredRTT;
RTO = (srtt >> 3) + rttvar;

Since the rto computed was in milliseconds granularity the timer structure was updated as follows:-

rto_val_ms = rtt_start_plus(&rttinfo);
upload_timer->it_interval.tv_sec = 0;
upload_timer->it_interval.tv_usec = 0;
upload_timer->it_value.tv_sec = rto_val_ms/1000;
upload_timer->it_value.tv_usec = (rto_val_ms%1000)*1000;

NOTE:- After rto updations, it is passed through rtt_minmax function to keep the maximum timeout between 1000 & 3000 msec.

3) TCP mechanishms implemented:-

A) 3 Way Handshake:-
Implemented a 3 way handshake between client and server to ensure connection between the server and the client.
The client sends the file name which is to be downloaded from the server along with its advertised window size.(First Handshake)
The server sends the reply along with its address and the port the client can connect to. (Second Handshake)
The client connects to the connection fd sent by the server and sends a (Third Handshake).

Each of these handshakes is backed by a timer. On expiry of the timer, the handshake is resent.

After handshake successfully completes, the server starts sending the data to the client.

B) Sliding Window :-

A Sliding Window mechanism has been implemented on both client and server. 

The server reads the data from the file, sends it to the client and then adds it to the sliding window.

The client has a main producer thread which reads data sent from the server into its sliding window and sends an ack back to the server and a consumer thread which sleeps as per the 'u' distribution value computed from the formula and then reads the data present in the sliding window.

The code for this is in the files server_sliding_window.c and client_sliding_window.c. The separate handling of sliding windows was necessary, because the windows behave differently on the sending and the receiving side. (Adding out of order packets is possible only on the client, etc.) This design decision was made to simplify the code.
 
We have designed each segment to have a header. The fields of this FUP (File Upload Protocol) header are as follows --
1. Type of segment.
2. Sequence Number
3. Acknowledgement Number
4. Advertised Window

The field "Type of Segment" was added to cleanly handle the different types of segments sent, in particular the FIN packet. The types of segments are as follows -- FH, SH, TH (for the first three handshakes), TX (For file data), ACK (for acknowledgement) and FIN (to signal end of file transfer). The header is same in both directions, that is the sender and receiver send and receive packets with the same header.

The total size of a segment is 512 bytes, as required.

C) Flow Control :-

With each ACK that it sends out, the client also informs the server of its advertised window size. By respecting this advertised window size, the server takes care not to swamp the client. The server sits in a send-send-send loop till it realizes that it can't send any more segments. This can happen if its own window is full, or it reaches the capacity of the advertised window size of the client. In addition to this, congestion statistics also play a role. We'll mention that in the next section.

Server sends at most min(server_window_size, adv_window) segments till it waits for acks to come. On receiving an ack for the last unacknowleged segment, the server removes the corresponding segment from the sliding window. If it receives a combined ack it removes all the segments which have been acked for.


D) Reliability:-

The server has a timeout mechanism to make sure that all packets eventually get transferred to the client unless max retries for a segment is reached. The max retries count is 12.


Drop Test:- A drop test has been simulated at the client end which drops the packet if the probability computed is less
than the drop_test_prob. This simulates "loss in the network". 

For time outs, RTT mechanisms and a persistent timer has been added.

RTT Calculations:-The RTO is calculated based on calculations specified in the handout. In case of retransmissions exponential backoff happens and the rto is doubled. This takes care of the network problems if any.

E) Persistent Timer:-
If the clients advertised window is 0, the server waits using the persistent timer till it can send more packets and client 
can eventually read. This is done to avoid a deadlock scenario where both the server and the client are waiting for the other one to send something.

F) Fast Retransmit:- If a duplicated ack is received 3 times, then it means that the network is fine as some packets did make to the 
client which were acked but perhaps the packets received were not in order. So a fast retransmission happens which resends the 
segment for which the ack comes.

G) Congestion Control:-

A congestion window has been implemented on the server side. This helps the server adapt to the changing network conditions.
Cwin, SSthreshold, client_advertised_window_size and server_sliding_window_size have been used to implement it.

H) Slow Start:-
Server starts with initial Cwin = 1 and ssthrehold = client_advertised_window_size.
On each successful ack it increases the Cwin by 1.
At any point of time the server can atmost send min(cwin - packets_in_transit, advertised_win - packets_in_transit, server_window_size)

On a timeout, Cwin=1, SSthres = [Cwin]/2. This is done to ensure that if network is congested then the packets are transmitted in 
accordance.

4) Closing of connections

To make sure that the server succesfully transfers the file to the client and client also comes to know about it, 
the server waits for all acks for which the segments have been sent. After that it sends a FIN to the client and waits 
for an ACK (effectively the FIN-ACK) from the client. The server waits max for 30 sec for the client to ack the FIN, failing which it closes the connection to the client.