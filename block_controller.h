#ifndef BLOCK_CONTROLLER_INCLUDED
#define BLOCK_CONTROLLER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : block_controller.h
//  Description    : This is the interface of the controller for the BLOCK
//                   memory system interface.
//
//  Author         : Patrick McDaniel
//

// Include
#include <stdint.h>

// These are the constants defining the size of the controllers
#define BLOCK_BLOCK_SIZE 65536
#define BLOCK_FRAME_SIZE 4096

// Type definitions
typedef uint64_t BlockXferRegister; // This is the value passed through the
    // controller register
typedef uint16_t BlockIndex; // Index number of block
typedef uint16_t BlockFrameIndex; // Frame index in current block
typedef char BlockFrame[BLOCK_FRAME_SIZE]; // A Frame
typedef BlockFrame BlockBlock[BLOCK_BLOCK_SIZE]; // A Block

/*

 BlockXferRegister Specification (See slides)

  Bits    Register (note that top is bit 0)
 ------   -----------------------------------
   0-7 - KY1 (Key Register 1)
  7-23 - FM1 (Key Register 2)
 24-55 - CS1 (Checksum register 1)
 56-63 - RT1 (Return code register 1)

*/

// These are the registers for the controller
typedef enum {

    BLOCK_REG_KY1 = 0, // Key 1 register (8 bits)
    BLOCK_REG_FM1 = 1, // Frame register 1 (16 bits)
    BLOCK_REG_CS1 = 2, // Checksum 1 register (32 bits)
    BLOCK_REG_RT1 = 3, // Return code 1 (8 bits)
    BLOCK_REG_MAXVAL = 4, // Maximum opcode value

} BlockRegisters;

// These are the opcode (instructions) for the controller
typedef enum {

    BLOCK_OP_INITMS = 0, // Initialize the memory interfaces
    BLOCK_OP_BZERO = 1, // Zero the block storage
    BLOCK_OP_RDFRME = 2, // Read the block frame
    BLOCK_OP_WRFRME = 3, // Write to the block frame
    BLOCK_OP_POWOFF = 4, // Power off the memory system
    BLOCK_OP_MAXVAL = 5, // Maximum opcode value

} BlockOpCodes;

// These are the possible return values in RT1
typedef enum {
    BLOCK_RET_SUCCESS = 0, // Success
    BLOCK_RET_ERROR = -1, // Generic error
    BLOCK_RET_CHECKSUM_ERROR = 2, // Checksum failed
} BlockReturnValues;

//
// Global Data

extern unsigned long BlockControllerLLevel; // Controller log level
extern unsigned long BlockDriverLLevel; // Driver log level
extern unsigned long BlockSimulatorLLevel; // Driver log level

//
// Functional Prototypes

BlockXferRegister block_io_bus(BlockXferRegister regstate, void* buf);
// This is the bus interface for communicating with controller

int block_unit_test(void);
// This function runs the unit tests for the block controller.

#endif
