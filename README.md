# Homework 2 - SUB-POST PROTOCOL

## HEADER
	* CLIENT ID - String that identifies a client
	* SYNC - BYTE value - representing the subject of the packet
	* DATA_TYPE - BYTE value - representing message type
	* TOPIC_LEN - BYTE value - representing the lenght of the topic which will be transmited after the header
	* MSG_LEN - TWO BYTE value - representing lenght of the message
## PAYLOAD
	Can be empty or of varied lenghts depending on the SYNC field of the header

### PACKET SUBJECT
	* SEND_MSG
		Sent by the server when sending a message
		Format: | Header | UDP_CLIENT_ADDR | TOPIC | MESSAGE |
	* START_CONN
		Send by the client after the TCP connection is established
		Format: | Header |
	* SUBSCRIBE
		Send by the client to subscribe to a topic without Store and Forward
		Format: | Header | Topic
	* SUBSCRIBE_SF
		Send by the client to subscribe to a topic with Store and Forward
		Format: | Header | Topic
	* UNSUBSCRIBE
		Send by the client to unsubscribe from a topic
		Format: | Header | Topic
	* REFUSE_CONN
		Send by the server when a second client tries to start a connection with the same id of a already connected client
		Format: | Header |
