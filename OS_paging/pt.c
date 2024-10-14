#include <stdint.h>
#include "os.h"

/* MACROS */
#define page_size 0b111111111
#define valid_bit 0b1
#define children 9
#define virt_page_bit_length 45
#define total_levels 5


/* Calculating entries for current level in the tree */
int calc_entry(uint64_t virt_frame, int level){
    return ((virt_frame >> (virt_page_bit_length - level*children)) & page_size);
}


/* Checking whether physical page is in memory, i.e valid */
int page_exists(uint64_t phy_page) {
    return (phy_page & valid_bit);
}


/* 
	UPDATING DEMAND	
*/

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {
    /* initialization */
	uint64_t* curr_phy_page_as_virt = phys_to_virt(pt << 12); //starting with the root 
	int curr_entry;

    /* Prefixing the tree from root to leaves */
	for (int i = 1; i < 5; i++) {
		curr_entry = calc_entry(vpn, i);
		if (!page_exists(curr_phy_page_as_virt[curr_entry])) {
			curr_phy_page_as_virt[curr_entry] = (alloc_page_frame() << 12) + 1; // +1 due to valid bit 
		}
		curr_phy_page_as_virt = phys_to_virt(curr_phy_page_as_virt[curr_entry] - 1); // -1 in order to cancel out the valid bit marking
                                                                                     // and refer to the original allocation
                                                                                     // from alloc_page_frame 
	}

    /* Abusing the array to store the mapping */
	int last_level_entry = calc_entry(vpn, total_levels);
	if (ppn == NO_MAPPING)
        curr_phy_page_as_virt[last_level_entry] = 0b0; // saying bye bye to previously allocated frame, ZEROING the ppn
    else 
        curr_phy_page_as_virt[last_level_entry] = (ppn << 12) + 1; // welcoming new mapping and validating the page
}


/*
    QUERYING DEMAND
*/

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
    /* initialization */
	uint64_t* curr_phy_page_as_virt = phys_to_virt(pt << 12); //starting with the root 
	int curr_entry;

    /* At any point - if page along the prefix tree is found invalid - return NO_MAPPING */
	for (int i = 1; i < 5; i++) {
		curr_entry = calc_entry(vpn, i);
		if (!page_exists(curr_phy_page_as_virt[curr_entry])) 
			return NO_MAPPING;

		curr_phy_page_as_virt = phys_to_virt(curr_phy_page_as_virt[curr_entry] - 1);
	}

    int last_level_entry = calc_entry(vpn, total_levels);

    return ( 
        page_exists(curr_phy_page_as_virt[last_level_entry]) ?
    (curr_phy_page_as_virt[last_level_entry] >> 12) : NO_MAPPING 
           ); 
}