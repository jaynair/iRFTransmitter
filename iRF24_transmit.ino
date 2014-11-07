/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example for Getting Started with nRF24L01+ radios. 
 *
 * This is an example of how to use the RF24 class.  Write this sketch to two 
 * different nodes.  Put one of the nodes into 'transmit' mode by connecting 
 * with the serial monitor and sending a 'T'.  The ping node sends the current 
 * time to the pong node, which responds by sending the value back.  The ping 
 * node can then see how long the whole cycle took.
 */
 
 // JAY: Use this sketch to transmit to the nRF24L01 on the Raspberry Pi

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
//#include <IRremote.h>
#include <HashMap.h>
//#include <IRremoteInt.h>

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10 

RF24 radio(9,10);

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0x7365727631LL, 0xF0F0F0F0E3LL };

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.  
//

// The various roles supported by this sketch
typedef enum { role_ping_out = 1, role_pong_back } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};

// The role of the current running sketch
role_e role = role_ping_out;
//IRsend irsend;
char strMessage[32] = "";

// JAY: Pin definition for LDR (Analog 7)
int light_intensity = 7;
int dark = 0;
int lastSeenCurrent;
const int current_command = 1; // format of data packet: commmand * 10000 + value

void setup(void)
{
  //
  // Print preamble
  //

  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/GettingStarted/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);
  printf("*** PRESS 'T' to begin transmitting to the other node\n\r");

  //
  // Setup and configure rf radio
  //

  radio.begin();

  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15,15);

  // optionally, reduce the payload size.  seems to
  // improve reliability
  //radio.setPayloadSize(8);

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

  //if ( role == role_ping_out )
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
    
  }
  //else
  {
    //radio.openWritingPipe(pipes[1]);
    //radio.openReadingPipe(1,pipes[0]);
  }

  //
  // Start listening
  //

  radio.startListening();

  //
  // Dump the configuration of the rf unit for debugging
  //

  radio.printDetails();
  
  // JAY 
  // Setup the Hash Map
  //  KEY: string from RasPi
  //  VALUE: IR Code
  
 // JAY for reading LDR
 pinMode(7,INPUT); 
 lastSeenCurrent = 0;
}

void loop(void)
{
  //
  // Ping out role.  Repeatedly send the current time
  //

  if (role == role_ping_out)
  {
    // First, stop listening so we can talk.
    radio.stopListening();

    // Take the time, and send it.  This will block until complete
    
     //[JAY] Current reading
    int current  = analogRead(0);
    if( current > (1.03*lastSeenCurrent) || current <(0.97*lastSeenCurrent))
    {
      lastSeenCurrent = current;
    
    //printf("CURRENT %d...",current);
    
    unsigned long data = current_command * 10000 + lastSeenCurrent;
      printf("Now sending %d...",data);
      bool ok = radio.write( &data, sizeof(unsigned long) );
      
      if (ok)
        printf("ok...");
      else
        printf("failed.\n\r");
  
      // Now, continue listening
      radio.startListening();
  
      // Wait here until we get a response, or timeout (250ms)
      unsigned long started_waiting_at = millis();
      bool timeout = false;
      while ( ! radio.available() && ! timeout )
        if (millis() - started_waiting_at > 200 )
          timeout = true;
  
      // Describe the results
      if ( timeout )
      {
        printf("Failed, response timed out.\n\r");
      }
      else
      {
        // Grab the response, compare, and send to debugging spew
        unsigned long got_time;
        radio.read( &got_time, sizeof(unsigned long) );
  
        // Spew it
        printf("Got response %lu, round-trip delay: %lu\n\r",got_time,millis()-got_time);
      }
    }
    else
    {
      radio.startListening();
    }
    // Try again 1s later
    delay(1000);
  }

  //
  // Pong back role.  Receive each packet, dump it out, and send it back
  //

  if ( role == role_pong_back )
  {
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      unsigned long received_index;
      bool done = false;
 

      // First, stop listening so we can talk
      radio.stopListening();

      // Send the final one back.
      radio.write( &received_index, sizeof(unsigned long) );
      printf("Sent response.\n\r");

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
    
    //JAY: Check LDR to see if lights switched off
    int intensity = analogRead(light_intensity)/4;
    
   
    
    if ((dark == 0) && (intensity < 30)){
      dark = 1;
      // irsend.sendNEC(551485695, 32);
       //delay(9000);
       //irsend.sendNEC(551489775, 32);
    }
    
    if ((dark == 1) && (intensity > 70)){
      dark = 0;
    }
        
  }

  //
  // Change roles
  //

  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    if ( c == 'T' && role == role_pong_back )
    {
      printf("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK\n\r");

      // Become the primary transmitter (ping out)
      role = role_ping_out;
      radio.openWritingPipe(pipes[0]);
      radio.openReadingPipe(1,pipes[1]);
    }
    else if ( c == 'R' && role == role_ping_out )
    {
      printf("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK\n\r");
      
      // Become the primary receiver (pong back)
      role = role_pong_back;
      radio.openWritingPipe(pipes[1]);
      radio.openReadingPipe(1,pipes[0]);
    }
  }
}
// vim:cin:ai:sts=2 sw=2 ft=cpp
