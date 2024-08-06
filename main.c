#include "sys/types.h"
#include "unistd.h"
#include "stdio.h"
#include "stdint.h"
#include "inttypes.h"
#include "time.h"

#include "can_linux.h"
#include "J73.h"

#define INTER_OPERATION_DELAY 20

/**  ________________________
  * |           |            |
  * | load_tlz  |     ECU    |
  * |___________|____________|______________________________________________
  * |           |            |                                              | app works 
  * | DM14_BOOT |            | want to alter the mem on a ecu, key is 0xFFF | app must save info
  * |___________|____________|                                              | that we must to flash
  * |           |            |                                              | in BOOT_DATA section
  * |           | DM15_SEED  | send proceed with seed to gen key            | in boot_data_t struct 
  * |___________|____________|______________________________________________|
  * |           |            |                                              | bootloader works
  * | DM14_KEY  |            | repeat 1st DM14 with key                     | 
  * |___________|____________|
  * |           |            |
  * |           | DM15_FFFF  | check and if ok answer with 0xFFFF seed
  * |___________|____________|
  * |           |            |
  * |DM17...DM17|            | 8B per pkg, max 2048B
  * |___________|____________|
  * |           |            |
  * | DM15_CMPLT|            | DM15 with status OP_COMPLETE 
  * |___________|____________|
 */

#define TARGET_ADDR 0x83
#define LOADER_ADDR 0xFA


/**  _____________________________________________
  * |              |              |               |
  * |     MCU1     |     MCU2     |     MCU3      |
  * |     0X08     |     0x18     |     0X28      |
  * |______________|______________|_______________|
 */

#define MCU_3_MEM_REGION (uint32_t) 0x08010000


void msleep(ms){
	usleep(ms * 1000);
}


J81_addr_claim_pkg_t addr_claim_pkg;
void J81_formur_this_addr(){
	uint32_t iden_num   = 2043; // random value set by manuf
	uint16_t manuf_code = 2042; // value if comp is not reg
	
	addr_claim_pkg.iden_num_lsb     = iden_num;
	addr_claim_pkg.iden_num_mid     = (iden_num >> 8);
	addr_claim_pkg.iden_num_msb     = (iden_num >> 16); 
	addr_claim_pkg.manuf_code_lsb   = (manuf_code); 
	addr_claim_pkg.manuf_code_msb   = (manuf_code);
	addr_claim_pkg.ecu_inst         = 0;
	addr_claim_pkg.fun_inst         = 0;
	addr_claim_pkg.fun              = 240; //Off-board diagnostic-service tool (B12)
	addr_claim_pkg.vehicle_sys      = 0; // non specific sys
	addr_claim_pkg.vehicle_sys_inst = 0; //offboard diafnostic service tool in most cases is 0
	addr_claim_pkg.industry_group   = AGRICULTURE_AND_FORESTRY_EQUIPMENT;
	addr_claim_pkg.arb_addr_cab     = 0; //J81 str 9 we here not fot resolve other addrs conflicts
}


void J81_addr_claim(){
	J81_formur_this_addr();

	J1939_ID_t addr_claim_id;
	formur_addr_claim_id(&addr_claim_id, LOADER_ADDR);
	
	can0_transmit(addr_claim_id.allmem, 8, (uint8_t*)&addr_claim_pkg);
}


uint32_t get_file_sz(FILE* file_ptr){ // in Bytes
	fseek(file_ptr, 0L, SEEK_END);
	uint32_t image_sz = ftell(file_ptr);
	rewind(file_ptr);
	return image_sz;
}


int pkg_is_target_addr_claim(uint32_t rx_id){
	J1939_ID_t rx_jid = (J1939_ID_t)rx_id;
	return rx_jid.PF == 238  &&
	       rx_jid.SA == TARGET_ADDR;
}


void wait_until_ecu_appear_in_network(){
	uint32_t rx_id;
	uint8_t  rx_dlc;
	uint8_t  rx_buf[8] = {};
	
	for(;;){
		can0_receive(&rx_id, &rx_dlc, rx_buf);
		if(pkg_from_target(rx_id, TARGET_ADDR)) break;
	}
} 


uint16_t get_seed_from_dm15(uint8_t* rx_buf){
	J1939_DM15_pkg_t* dm15_pkg_ptr = (J1939_DM15_pkg_t*) rx_buf;
	uint16_t seed = dm15_pkg_ptr->seed_lsb | (dm15_pkg_ptr->seed_msb << 8);
	return seed;
}


uint16_t wait_responce_on_want_boot_with_seed(){
	uint32_t rx_id;
	uint8_t  rx_dlc;
	uint8_t  rx_buf[8] = {};  
    
	for(;;){
		can0_receive(&rx_id, &rx_dlc, rx_buf);
		if(pkg_is_dm15(rx_id) && pkg_from_target(rx_id, TARGET_ADDR))
			break; }
	return get_seed_from_dm15(rx_buf);
}


uint16_t cool_pswd_from_seed(uint16_t seed){
	(void) seed;
	return 0xFEED;
}


void send_want_boot_with_key(uint16_t seed, uint16_t image_part_sz, uint32_t ptr_in_ecu_mem){
	J1939_ID_t       dm14_id;
	J1939_DM14_pkg_t dm14_pkg;
	
	formur_dm14_id(&dm14_id, TARGET_ADDR);
	formur_dm14_pkg(&dm14_pkg, image_part_sz, seed, ptr_in_ecu_mem);
	
	can0_transmit(dm14_id.allmem, 8, (uint8_t*)&dm14_pkg.allmem);
}


void wait_responce_on_want_boot_with_ffff(){
	uint32_t rx_id;
	uint8_t  rx_dlc; 
	uint8_t  rx_buf[8] = {};

	for(;;){ 
		can0_receive(&rx_id, &rx_dlc, rx_buf); 
		if(pkg_is_dm15(rx_id) && pkg_from_target(rx_id, TARGET_ADDR)) 
			break; }
}


void send_complete_pkg(){
	J1939_DM15_pkg_t dm15_pkg;
	J1939_ID_t dm15_id;
  
	formur_dm15_id(&dm15_id, TARGET_ADDR);
	formur_dm15_pkg(&dm15_pkg, 0, 0xFFFF, DM15_STATUS_OP_COMPLETED);
  
	can0_transmit(dm15_id.allmem, 8, (uint8_t*)&dm15_pkg.allmem);
}


uint32_t send_image_pkgs_in_session_c = 0;
void send_image_pkgs_in_session(uint16_t image_part_sz, FILE* image_file_ptr, uint32_t ecu_mem_addr){
	uint8_t image_buf[8];
	uint8_t tx_dlc = 8;
	uint8_t t_offset = 0;

	for(int32_t image_part_sz_remain = image_part_sz; image_part_sz_remain > 0; image_part_sz_remain -= 8){
		msleep(INTER_OPERATION_DELAY);
		fprintf(stderr, "send_image_pkgd_in_session num: %d\n", ++send_image_pkgs_in_session_c);
		J1939_ID_t dm17_id;
		formur_dm17_id(&dm17_id, TARGET_ADDR);
        
		if(tx_dlc > image_part_sz_remain) tx_dlc = image_part_sz_remain;
		fread(image_buf, sizeof(image_buf[0]), tx_dlc, image_file_ptr);
    
		can0_transmit(dm17_id.allmem, tx_dlc, image_buf);
		++t_offset;
	}
}


uint32_t send_image_session_c = 0;
void send_image_session(uint32_t image_part_sz, FILE* image_file_ptr, uint32_t ptr_in_ecu_mem){
	printf(stderr, "\tsend_image_session num: %d\n", ++send_image_session_c);
	send_want_boot_with_key(0xFFFF, image_part_sz, ptr_in_ecu_mem);
	uint16_t seed = wait_responce_on_want_boot_with_seed(); //wait until ecu witch to bootloader
	msleep(42); 
	send_want_boot_with_key(cool_pswd_from_seed(seed), image_part_sz, ptr_in_ecu_mem);
	send_want_boot_with_key(cool_pswd_from_seed(seed), image_part_sz, ptr_in_ecu_mem);
	wait_responce_on_want_boot_with_ffff();
	msleep(INTER_OPERATION_DELAY);
	send_image_pkgs_in_session(image_part_sz, image_file_ptr, ptr_in_ecu_mem);
	msleep(INTER_OPERATION_DELAY);
}


void send_image(uint32_t image_sz, FILE* image_file_ptr, uint32_t ecu_mem_addr){
	uint8_t full_sz_pkg_num = image_sz / 2048;
	for(uint16_t remain_pkg_num = full_sz_pkg_num; remain_pkg_num > 0; --remain_pkg_num){
		msleep(INTER_OPERATION_DELAY);
		send_image_session(2048, image_file_ptr, ecu_mem_addr);
		ecu_mem_addr+=2048;
	}
    
	msleep(INTER_OPERATION_DELAY);
	send_image_session(image_sz % 2048, image_file_ptr, ecu_mem_addr);
	msleep(INTER_OPERATION_DELAY);
	send_complete_pkg();
}


int main(int argc, char** argv){
	
	if(argc < 1){ 
		fprintf(stderr, "image files required, call: exe /path/to/image_mcu1.bin");
		abort();
	}
	
	FILE *image_file_ptr_mcu_1;	
	image_file_ptr_mcu_1 = fopen(argv[1], "rb");
	uint32_t image_sz_mcu_1 = get_file_sz(image_file_ptr_mcu_1);

	can0_init();
	//can0_set_filter(); TODO listen TARGET_ADDR pkgs
	J81_addr_claim();
	msleep(INTER_OPERATION_DELAY);
	fprintf(stderr, "image sz in Bytes: %d\n", image_sz_mcu_1);
    
	wait_until_ecu_appear_in_network(); 
	fprintf(stderr, "load start\n");
	msleep(INTER_OPERATION_DELAY); 
	send_image(image_sz_mcu_1, image_file_ptr_mcu_1, MCU_3_MEM_REGION);

	fprintf(stderr, "load finish\n");
	can0_deinit();
	return 0;
}
