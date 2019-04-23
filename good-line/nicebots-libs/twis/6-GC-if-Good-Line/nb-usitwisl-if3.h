/*
 *  Timonel USI TWI Interrupt-Free Driver
 *  Author: Gustavo Casanova
 *  ...........................................
 *  File: nb-usitwisl.h
 *  ........................................... 
 *  Version: 1.1 / 2019-04-23
 *  gustavo.casanova@nicebots.com
 *  ........................................... 
 *  Based on work by Atmel (AVR312) et others
 *  ...........................................
 */

#ifndef _NB_USITWISL_IF_H_
#define _NB_USITWISL_IF_H_

// Includes
#include <avr/interrupt.h>
#include <stdbool.h>

// Driver buffer definitions
// Allowed RX buffer sizes: 1, 2, 4, 8, 16, 32, 64, 128 or 256
#ifndef TWI_RX_BUFFER_SIZE
#define TWI_RX_BUFFER_SIZE (16)
#endif

#define TWI_RX_BUFFER_MASK (TWI_RX_BUFFER_SIZE - 1)

#if (TWI_RX_BUFFER_SIZE & TWI_RX_BUFFER_MASK)
#error TWI RX buffer size is not a power of 2
#endif

// Allowed TX buffer sizes: 1, 2, 4, 8, 16, 32, 64, 128 or 256
#ifndef TWI_TX_BUFFER_SIZE
#define TWI_TX_BUFFER_SIZE (16)
#endif

#define TWI_TX_BUFFER_MASK (TWI_TX_BUFFER_SIZE - 1)

#if (TWI_TX_BUFFER_SIZE & TWI_TX_BUFFER_MASK)
#error TWI TX buffer size is not a power of 2
#endif

// Device Dependent Defines
#if defined(__AVR_ATtiny25__) | \
    defined(__AVR_ATtiny45__) | \
    defined(__AVR_ATtiny85__)
#define DDR_USI DDRB
#define PORT_USI PORTB
#define PIN_USI PINB
#define PORT_USI_SDA PB0
#define PORT_USI_SCL PB2
#define PIN_USI_SDA PINB0
#define PIN_USI_SCL PINB2
#define TWI_START_COND_FLAG	USISIF	/* This status register flag indicates that an I2C START condition occurred on the bus (can trigger an interrupt) */
#define USI_OVERFLOW_FLAG USIOIF	/* This status register flag indicates that the bits reception or transmission is complete (can trigger an interrupt) */
#define TWI_STOP_COND_FLAG USIPF	/* This status register flag indicates that an I2C STOP condition occurred on the bus */
#define TWI_COLLISION_FLAG USIDC	/* This status register flag indicates that a data output collision occurred on the bus */
#define TWI_START_COND_INT USISIE	/* This control register bit defines whether an I2C START condition will trigger an interrupt */
#define USI_OVERFLOW_INT USIOIE		/* This control register bit defines whether an USI 4-bit counter overflow will trigger an interrupt */
#endif

#endif /* _NB_USITWISL_IF_H_ */

// Device modes
typedef enum {
	STATE_CHECK_ADDRESS,
    STATE_SEND_DATA,
    STATE_WAIT_ACK_AFTER_SEND_DATA,
    STATE_CHECK_ACK_AFTER_SEND_DATA,
    STATE_WAIT_DATA_RECEPTION,
    STATE_RECEIVE_DATA_AND_SEND_ACK   
} OperationalState;

// Global library variables
OperationalState device_state;
uint8_t rx_buffer[TWI_RX_BUFFER_SIZE];
uint8_t rx_head;
uint8_t rx_tail;
uint8_t rx_count;
uint8_t tx_buffer[TWI_TX_BUFFER_SIZE];
uint8_t tx_head;
uint8_t tx_tail;
uint8_t tx_count;

// Prototypes
void UsiTwiTransmitByte(uint8_t);
uint8_t UsiTwiReceiveByte(void);
void ReceiveEvent(uint8_t);
void RequestEvent(void);

// Helper function prototypes
inline void UsiTwiSlaveInit(void) __attribute__((always_inline));
inline bool UsiTwiDataInTransmitBuffer(void) __attribute__((always_inline));
inline uint8_t UsiTwiAmountDataInReceiveBuffer(void) __attribute__((always_inline));
inline void FlushTwiBuffers(void) __attribute__((always_inline));

// I2C handlers prototypes (These functions replace the USI hardware interrupts)
inline void UsiStartHandler(void) __attribute__((always_inline));
inline uint8_t UsiOverflowHandler(void) __attribute__((always_inline));

// USI direction setting prototypes
inline void SET_USI_SDA_AS_OUTPUT() __attribute__((always_inline));
inline void SET_USI_SDA_AS_INPUT() __attribute__((always_inline));
inline void SET_USI_SCL_AS_OUTPUT() __attribute__((always_inline));
inline void SET_USI_SCL_AS_INPUT() __attribute__((always_inline));
inline void SET_USI_SDA_AND_SCL_AS_OUTPUT() __attribute__((always_inline));
inline void SET_USI_SDA_AND_SCL_AS_INPUT() __attribute__((always_inline));

// USI basic TWI operations prototypes
inline void SET_USI_TO_SEND_ACK() __attribute__((always_inline));
inline void SET_USI_TO_WAIT_ACK() __attribute__((always_inline));
inline void SET_USI_TO_WAIT_FOR_START_COND_AND_ADDRESS() __attribute__((always_inline));
inline void SET_USI_TO_SEND_DATA() __attribute__((always_inline));
inline void SET_USI_TO_RECEIVE_DATA() __attribute__((always_inline));

// Data callback prototypes
inline void DATA_RECEIVED_FROM_MASTER_CALLBACK() __attribute__((always_inline));
inline void DATA_REQUESTED_BY_MASTER_CALLBACK() __attribute__((always_inline));
inline void STOP_CONDITION_RECEIVED_CALLBACK() __attribute__((always_inline));

/////////////////////////////////////////////////////////////////////////////
//////////////////////// ALL ALL ALL ALL ALL ALL ALL ////////////////////////
/////////////////////////////////////////////////////////////////////////////

// Function FlushTwiBuffers
inline void FlushTwiBuffers(void) {
    rx_tail = 0;
    rx_head = 0;
    rx_count = 0;
    tx_tail = 0;
    tx_head = 0;
    tx_count = 0;
}

// Function UsiTwiSlaveInit
inline void UsiTwiSlaveInit(void) {
	/* In Two Wire mode (USIWM1, USIWM0 = 1X), the slave USI will pull SCL
       low when a start condition is detected or a counter overflow (only
       for USIWM1, USIWM0 = 11).  This inserts a wait state.  SCL is released
       by the ISRs (USI_START_vect and USI_OVERFLOW_vect).
	*/
    FlushTwiBuffers();
	SET_USI_SDA_AND_SCL_AS_OUTPUT(); /* Set SCL and SDA as output */
    PORT_USI |= (1 << PORT_USI_SCL); /* Set SCL high */
    PORT_USI |= (1 << PORT_USI_SDA); /* Set SDA high */
	SET_USI_SDA_AS_INPUT(); 		 /* Set SDA as input */
	SET_USI_TO_WAIT_FOR_START_COND_AND_ADDRESS(); /* Wait for TWI start condition and address from master */
}

// Function UsiTwiDataInTransmitBuffer
inline bool UsiTwiDataInTransmitBuffer(void) { 
    return tx_count; /* Return 0 (false) if the receive buffer is empty */
}

// Function UsiTwiTransmitByte
void UsiTwiTransmitByte(uint8_t data_byte) {
    while (tx_count++ == TWI_TX_BUFFER_SIZE) {
		/* Wait until there is free space in the TX buffer */
    };
    tx_buffer[tx_head] = data_byte; /* Write the data byte into the TX  buffer */
    tx_head = (tx_head + 1) & TWI_TX_BUFFER_MASK;
}

// Function UsiTwiReceiveByte
uint8_t UsiTwiReceiveByte(void) {
    uint8_t received_byte;
    while (!rx_count--) {
        /* Wait until a byte is received into the RX buffer */
    };
    received_byte = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) & TWI_RX_BUFFER_MASK; /* Calculate the buffer index */
    return received_byte; /* Return the received byte from the buffer */
}

// Function UsiTwiAmountDataInReceiveBuffer
inline uint8_t UsiTwiAmountDataInReceiveBuffer(void) {
    return rx_count;
}

// Function UsiStartHandler (Interrupt-like handler function)
inline void UsiStartHandler() {
    device_state = STATE_CHECK_ADDRESS; /* Set default starting conditions for a new TWI package */
	SET_USI_SDA_AS_INPUT(); /* Float the SDA line */
    while ((PIN_USI & (1 << PORT_USI_SCL)) && (!(PIN_USI & (1 << PORT_USI_SDA)))) {
		/* Wait for SCL to go low to ensure the start condition has completed (the
		   start detector will hold SCL low ).
		*/
	}
	// If a stop condition arises then leave this function to prevent waiting forever.
	// Don't use USISR to test for stop condition as in application note AVR312
	// because the stop condition flag is going to be set from the last TWI sequence.
	if (!(PIN_USI & (1 << PIN_USI_SDA))) {	/*** Stop condition NOT DETECTED ***/
		// Keep start condition interrupt enabled to detect RESTART
		USICR = (1 << TWI_START_COND_INT) | (1 << USI_OVERFLOW_INT) | /* Enable start condition interrupt, disable overflow interrupt */
				(1 << USIWM1) | (1 << USIWM0) | /* Set USI in Two-wire mode, hold SCL low when the 4-bit counter overflows */
				(1 << USICS1) | (0 << USICS0) | (0 << USICLK) | /* Clock Source = External (positive edge) for data register, External (both edges) for 4-Bit counter */
				(0 << USITC); /* No toggle clock-port pin (SCL) */
    } else { 								/*** Stop condition DETECTED ***/
		/* Configure USI control register */
		USICR = (1 << TWI_START_COND_INT) | (0 << USI_OVERFLOW_INT) | /* Enable start condition interrupt, disable overflow interrupt */
				(1 << USIWM1) | (0 << USIWM0) | /* Set USI in Two-wire mode, no SCL hold when the 4-bit counter overflows */
				(1 << USICS1) | (0 << USICS0) | (0 << USICLK) | /* Clock Source = External (positive edge) for data register, External (both edges) for 4-Bit counter */
				(0 << USITC); /* No toggle clock-port pin (SCL) */				
    }
	/* Clear all USI status register interrupt flags to prepare for new start conditions */
	USISR = (1 << TWI_START_COND_FLAG) |
			(1 << USI_OVERFLOW_FLAG) |
			(1 << TWI_STOP_COND_FLAG) |
			(1 << TWI_COLLISION_FLAG) |
			(0x0 << USICNT0); /* Reset status register 4-bit counter to shift 8 bits (data byte to be received) */  
}

// Function UsiOverflowHandler (Interrupt-like handler function)
inline uint8_t UsiOverflowHandler() {
    switch (device_state) {
        // Check address mode: check received address and send ACK (and next STATE_SEND_DATA) if OK,
        // else reset USI
        case STATE_CHECK_ADDRESS: {
            if ((USIDR == 0) || ((USIDR >> 1) == TWI_ADDR)) {
                if (USIDR & 0x01) {             /* If lsbit = 1: Send data to master */
                    DATA_REQUESTED_BY_MASTER_CALLBACK();
                    device_state = STATE_SEND_DATA;
                } else {                        /* If lsbit = 0: Receive data from master */
                    device_state = STATE_WAIT_DATA_RECEPTION;
                }
                SET_USI_TO_SEND_ACK();
            }
            else {
                SET_USI_TO_WAIT_FOR_START_COND_AND_ADDRESS();
            }
            break;
        }
        // Master write data mode: check reply and goto STATE_SEND_DATA if OK,
        // else reset USI
        case STATE_CHECK_ACK_AFTER_SEND_DATA: {
            if (USIDR) {
                // If NACK, the master does not want more data
                SET_USI_TO_WAIT_FOR_START_COND_AND_ADDRESS();

                // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
                //                                                >>
                return(true); // Enable slow operations in main!    >>
                //                                                >> 
                // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
             }
        }
        // From here we just drop straight into STATE_SEND_DATA if the
        // master sent an ACK.
        // Copy data from buffer to USIDR and set USI to shift byte
        // Next STATE_WAIT_ACK_AFTER_SEND_DATA
        case STATE_SEND_DATA: {
            // Get data from Buffer
            if (tx_count--) {
                USIDR = tx_buffer[tx_tail];
                tx_tail = (tx_tail + 1) & TWI_TX_BUFFER_MASK;
            } else {
                // The buffer is empty
                SET_USI_TO_WAIT_ACK();  // This might be necessary sometimes see http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&p=805227#805227
                SET_USI_TO_WAIT_FOR_START_COND_AND_ADDRESS();
                return(false);
            }
            device_state = STATE_WAIT_ACK_AFTER_SEND_DATA;
            SET_USI_TO_SEND_DATA();
            break;
        }
        // Set USI to sample ACK/NACK reply from master
        // Next STATE_CHECK_ACK_AFTER_SEND_DATA
        case STATE_WAIT_ACK_AFTER_SEND_DATA: {
            device_state = STATE_CHECK_ACK_AFTER_SEND_DATA;
            SET_USI_TO_WAIT_ACK();
            break;
        }
        // Read data mode: set USI to sample data from master, next
        // STATE_RECEIVE_DATA_AND_SEND_ACK
        case STATE_WAIT_DATA_RECEPTION: {
            device_state = STATE_RECEIVE_DATA_AND_SEND_ACK;
            SET_USI_TO_RECEIVE_DATA();
            break;
        }
        // Take data from USIDR and send ACK
        // Next STATE_WAIT_DATA_RECEPTION
        case STATE_RECEIVE_DATA_AND_SEND_ACK: {
            // put data into buffer
            // check buffer size
            if (rx_count++ < TWI_RX_BUFFER_SIZE) {
                rx_buffer[rx_head] = USIDR;
                rx_head = (rx_head + 1) & TWI_RX_BUFFER_MASK;
            } else {
                /* Overrun, drop data */
            }
            // Next STATE_WAIT_DATA_RECEPTION
            device_state = STATE_WAIT_DATA_RECEPTION;
            SET_USI_TO_SEND_ACK();
            break;
        }
    }
    USISR |= (1 << USI_OVERFLOW_FLAG); /* Clear the 4-bit counter overflow flag in USI status register to prepare for new interrupts */
    return(false);
}

// -----------------------------------------------------
// USI direction setting functions
// -----------------------------------------------------
inline void SET_USI_SDA_AS_OUTPUT() {
	DDR_USI |=  (1 << PORT_USI_SDA);
}
// -----------------------------------------------------
inline void SET_USI_SDA_AS_INPUT() {
	DDR_USI &= ~(1 << PORT_USI_SDA);
}
// -----------------------------------------------------
inline void SET_USI_SCL_AS_OUTPUT() {
	DDR_USI |=  (1 << PORT_USI_SCL);
}
// -----------------------------------------------------
inline void SET_USI_SCL_AS_INPUT() {
	DDR_USI &= ~(1 << PORT_USI_SCL);
}
// -----------------------------------------------------
inline void SET_USI_SDA_AND_SCL_AS_OUTPUT() {
	DDR_USI |= (1 << PORT_USI_SDA) | (1 << PORT_USI_SCL);
}
// -----------------------------------------------------
inline void SET_USI_SDA_AND_SCL_AS_INPUT() {
	DDR_USI &= ~((1 << PORT_USI_SDA) | (1 << PORT_USI_SCL));
}

// -----------------------------------------------------
// USI basic TWI operations functions
// -----------------------------------------------------
inline void SET_USI_TO_SEND_ACK() {
	USIDR = 0;	/* Set the USI data register for sending an ACK (TWI ACK = low) */
	SET_USI_SDA_AS_OUTPUT(); /* Drive the SDA line */
	/* Clear all USI status register interrupt flags, except start condition (so SCL clock line is NOT released???) */
	USISR = (0 << TWI_START_COND_FLAG) |
			(1 << USI_OVERFLOW_FLAG) |
			(1 << TWI_STOP_COND_FLAG) |
			(1 << TWI_COLLISION_FLAG) |
			(0x0E << USICNT0); /* Set status register 4-bit counter to shift 1 bit (ACK bit) */
}
// -----------------------------------------------------
inline void SET_USI_TO_WAIT_ACK() {
	USIDR = 0;	/* Clear the USI data register to receive an ACK */
	SET_USI_SDA_AS_INPUT(); /* Float the SDA line */
	/* Clear all USI status register interrupt flags, except start condition */
	USISR = (0 << TWI_START_COND_FLAG) |
			(1 << USI_OVERFLOW_FLAG) |
			(1 << TWI_STOP_COND_FLAG) |
			(1 << TWI_COLLISION_FLAG) |
			(0x0E << USICNT0); /* Set status register 4-bit counter to shift 1 bit (ACK bit) */	
}
// -----------------------------------------------------
inline void SET_USI_TO_WAIT_FOR_START_COND_AND_ADDRESS() {
	/* Configure USI control register */
	USICR = (1 << TWI_START_COND_INT) | (0 << USI_OVERFLOW_INT) | /* Enable start condition interrupt, disable overflow interrupt */
		    (1 << USIWM1) | (0 << USIWM0) | /* Set USI in Two-wire mode, no SCL hold when the 4-bit counter overflows */
			(1 << USICS1) | (0 << USICS0) | (0 << USICLK) | /* Clock Source = External (positive edge) for data register, External (both edges) for 4-Bit counter */
			(0 << USITC); /* No toggle clock-port pin (SCL) */
	/* Clear all USI status register interrupt flags, except start condition */
	USISR = (0 << TWI_START_COND_FLAG) |
			(1 << USI_OVERFLOW_FLAG) |
			(1 << TWI_STOP_COND_FLAG) |
			(1 << TWI_COLLISION_FLAG) |
			(0x0 << USICNT0); /* Reset status register 4-bit counter to shift 8 bits (address byte to be received) */	 		
}
// -----------------------------------------------------
inline void SET_USI_TO_SEND_DATA() {
	SET_USI_SDA_AS_OUTPUT(); /* Drive the SDA line */
	/* Clear all USI status register interrupt flags, except start condition */
	USISR = (0 << TWI_START_COND_FLAG) |
			(1 << USI_OVERFLOW_FLAG) |
			(1 << TWI_STOP_COND_FLAG) |
			(1 << TWI_COLLISION_FLAG) |
			(0x0 << USICNT0); /* Reset status register 4-bit counter to shift 8 bits (data byte to be transmitted) */		
}
// -----------------------------------------------------
inline void SET_USI_TO_RECEIVE_DATA() {
	SET_USI_SDA_AS_INPUT(); /* Float the SDA line */
	/* Clear all USI status register interrupt flags, except start condition */
	USISR = (0 << TWI_START_COND_FLAG) |
			(1 << USI_OVERFLOW_FLAG) |
			(1 << TWI_STOP_COND_FLAG) |
			(1 << TWI_COLLISION_FLAG) |
			(0x0 << USICNT0); /* Reset status register 4-bit counter to shift 8 bits (data byte to be received) */		
	
}

// -----------------------------------------------------
// Data callback functions
// -----------------------------------------------------
inline void DATA_RECEIVED_FROM_MASTER_CALLBACK() {
    
    ReceiveEvent(UsiTwiAmountDataInReceiveBuffer());
    
	//if (Usi_onReceivePtr) {
	//	if (UsiTwiAmountDataInReceiveBuffer()) {
	//		Usi_onReceivePtr(UsiTwiAmountDataInReceiveBuffer());
	//	}
	//}
}
// -----------------------------------------------------
inline void DATA_REQUESTED_BY_MASTER_CALLBACK() {
	DATA_RECEIVED_FROM_MASTER_CALLBACK();
	
    RequestEvent();
    
    //if(Usi_onRequestPtr) {
	//	Usi_onRequestPtr();
    //}
}
// -----------------------------------------------------
inline void STOP_CONDITION_RECEIVED_CALLBACK() {
    if (USISR & (1 << TWI_STOP_COND_FLAG)) {
		DATA_RECEIVED_FROM_MASTER_CALLBACK();
    } 	
}
