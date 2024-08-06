#include "can_linux.h"
#include "inttypes.h"

int s; 
struct sockaddr_can addr;
struct ifreq ifr;

void print_msg(uint32_t id, uint8_t* payload, uint8_t DLC){
	J1939_ID_t j_id = *(J1939_ID_t*)&id;
	fprintf(stderr, "sa: %02X ", j_id.SA);
	fprintf(stderr, "da: %02X ", j_id.PS);
	fprintf(stderr, "pf: %02X ", j_id.PF);
	fprintf(stderr, "   payload: ");
	for(int i = 0; i <DLC; ++i)
		fprintf(stderr, " %02X", payload[i]);
	fprintf(stderr, "\n");
} 

int can0_init(){
	if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("Socket");
		abort();}
	
	strcpy(ifr.ifr_name, "can0" );
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind");
		abort()}
}

int can0_transmit(uint32_t id, uint8_t dlc, uint8_t* buf){
	struct can_frame frame;
 	frame.can_id = id | CAN_EFF_FLAG;
	frame.can_dlc = dlc;
	for (uint8_t i = 0; i < dlc; ++i)
		frame.data[i] = buf[i];
	fprintf(stderr, "out: "); print_msg(id, buf, dlc);
	
	if (write(s, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
		perror("Write");
		abort();}
}

void can0_set_filter(uint32_t can_id, uint32_t can_mask){
	struct can_filter filter[1];
	filter[0].can_id   = can_id;
	filter[0].can_mask = can_mask;
	setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter));
}

int can0_receive(uint32_t* get_id, uint8_t* get_dlc, uint8_t* buf){
	struct can_frame frame;
	uint32_t nbytes = read(s, &frame, sizeof(struct can_frame));
  
	if (nbytes < 0) {
		perror("Read");
		abort();}
	
	*get_id  = frame.can_id;
	*get_dlc = frame.can_dlc;

	for (uint8_t i = 0; i < frame.can_dlc; i++)
		buf[i] = frame.data[i];
    
	fprintf(stderr, "in : "); print_msg(*get_id, buf, *get_dlc);
}

int can0_deinit(){
	if (close(s) < 0) {
		perror("Close");
		abort();}
}
