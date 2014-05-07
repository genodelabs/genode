//********************************************************************
// MINIMUM ATA LOW LEVEL I/O DRIVER -- MINDRVR.C
//
// by Hale Landis (hlandis@ata-atapi.com)
//
// There is no copyright and there are no restrictions on the use
// of this ATA Low Level I/O Driver code.  It is distributed to
// help other programmers understand how the ATA device interface
// works and it is distributed without any warranty.  Use this
// code at your own risk.
//
// Minimum ATA Driver (MINDRVR) is a subset of ATADRVR. MINDRVR
// has a single header file and a single C file. MINDRVR can
// be used as the starting point for an ATADRVR for an embedded
// system. NOTE all the places in the MINDRVR.H and MINDRVR.C files
// where there is a comment containing the string "!!!".
//
// Use the header file mindrvr.h in any C files that call MINDRVR
// functions.
//
// This code is based on the ATA/ATAPI-4,-5 and -6 standards and
// on interviews with various ATA controller and drive designers.
//
// Note that MINDRVR does not support ATA CHS addressing.
//
// Most of the MINDRVR code is standard C code and should compile
// using any C compiler. It has been tested using Borland C/C++ 4.5.
//
// This C source file is the header file for the driver
// and is used in the MINDRVR.C files and must also be used
// by any program using the MINDRVR code/functions.
//********************************************************************

#include "mindrvr.h"
#include "pio.h"
//**************************************************************
//
// !!! data that functions outside of MINDRVR must use
//
// Note that there is no actual "interrupt handler" provide in
// MINDRVR. The interrupt handler is usually a small function that
// is very system specific. However, MINDRVR expects that interrupt
// handler function to provide some status data at the time the
// interrupt handler is executed.
//
// In many systems, including PCI bus based systems, when an
// interrupt is received from an ATA controller, the interrupt
// handler must acknowledge the interrupt by reading both the
// ATA/ATAPI device Status register and the controller status
// register. This status must be stored here so that MINDRVR
// can use it.
//
//**************************************************************

unsigned char int_ata_status;    // ATA status read by interrupt handler

unsigned char int_bmide_status;  // BMIDE status read by interrupt handler

unsigned char int_use_intr_flag = INT_DEFAULT_INTERRUPT_MODE;

struct REG_CMD_INFO reg_cmd_info;

int reg_config_info[2];

unsigned char * pio_bmide_base_addr;

unsigned char * pio_reg_addrs[9] =
{
   PIO_BASE_ADDR1 + 0,  // [0] CB_DATA
   PIO_BASE_ADDR1 + 1,  // [1] CB_FR & CB_ER
   PIO_BASE_ADDR1 + 2,  // [2] CB_SC
   PIO_BASE_ADDR1 + 3,  // [3] CB_SN
   PIO_BASE_ADDR1 + 4,  // [4] CB_CL
   PIO_BASE_ADDR1 + 5,  // [5] CB_CH
   PIO_BASE_ADDR1 + 6,  // [6] CB_DH
   PIO_BASE_ADDR1 + 7,  // [7] CB_CMD & CB_STAT
   PIO_BASE_ADDR2 + 0   // [8] CB_DC & CB_ASTAT
} ;

unsigned char pio_xfer_width = PIO_DEFAULT_XFER_WIDTH;

//**************************************************************
//
// functions internal and private to MINDRVR
//
//**************************************************************

static void sub_setup_command( void );
static void sub_trace_command( void );
static int sub_select( unsigned char dev );
static void sub_wait_poll( unsigned char we, unsigned char pe );

//static unsigned char pio_inbyte( unsigned char addr );
//static void pio_outbyte( int addr, unsigned char data );
//static unsigned int pio_inword( unsigned char addr );
//static void pio_outword( int addr, unsigned int data );
//static unsigned long pio_indword( unsigned char addr );
//static void pio_outdword( int addr, unsigned long data );
static void pio_drq_block_in( unsigned char addrDataReg,
                              unsigned char * bufAddr,
                              long wordCnt );
static void pio_drq_block_out( unsigned char addrDataReg,
                               unsigned char * bufAddr,
                               long wordCnt );
static void pio_rep_inbyte( unsigned char addrDataReg,
                            unsigned char * bufAddr,
                            long byteCnt );
static void pio_rep_outbyte( unsigned char addrDataReg,
                             unsigned char * bufAddr,
                             long byteCnt );
static void pio_rep_inword( unsigned char addrDataReg,
                            unsigned char * bufAddr,
                            long wordCnt );
static void pio_rep_outword( unsigned char addrDataReg,
                             unsigned char * bufAddr,
                             long wordCnt );
static void pio_rep_indword( unsigned char addrDataReg,
                             unsigned char * bufAddr,
                             long dwordCnt );
static void pio_rep_outdword( unsigned char addrDataReg,
                              unsigned char * bufAddr,
                              long dwordCnt );
/*
static unsigned char pio_readBusMstrCmd( void );
static unsigned char pio_readBusMstrStatus( void );
static void pio_writeBusMstrCmd( unsigned char x );
static void pio_writeBusMstrStatus( unsigned char x );
*/

static long tmr_cmd_start_time;     // command start time
static void tmr_set_timeout( void );
static int tmr_chk_timeout( void );

// This macro provides a small delay that is used in several
// places in the ATA command protocols:

#define DELAY400NS  { pio_inbyte( CB_ASTAT ); pio_inbyte( CB_ASTAT );  \
                      pio_inbyte( CB_ASTAT ); pio_inbyte( CB_ASTAT ); }

//*************************************************************
//
// reg_config() - Check the host adapter and determine the
//                number and type of drives attached.
//
// This process is not documented by any of the ATA standards.
//
// Infomation is returned by this function is in
// reg_config_info[] -- see MINDRVR.H.
//
//*************************************************************

int reg_config( void )

{
   int numDev = 0;
   unsigned char sc;
   unsigned char sn;
   unsigned char cl;
   unsigned char ch;
   unsigned char st;
   unsigned char dc;

   // setup register values

   dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );

   // reset Bus Master Error bit

   pio_writeBusMstrStatus( BM_SR_MASK_ERR );

   // assume there are no devices

   reg_config_info[0] = REG_CONFIG_TYPE_NONE;
   reg_config_info[1] = REG_CONFIG_TYPE_NONE;

   // set up Device Control register

   pio_outbyte( CB_DC, dc );

   // lets see if there is a device 0

   pio_outbyte( CB_DH, CB_DH_DEV0 );
   DELAY400NS;
   pio_outbyte( CB_SC, 0x55 );
   pio_outbyte( CB_SN, 0xaa );
   pio_outbyte( CB_SC, 0xaa );
   pio_outbyte( CB_SN, 0x55 );
   pio_outbyte( CB_SC, 0x55 );
   pio_outbyte( CB_SN, 0xaa );
   sc = pio_inbyte( CB_SC );
   sn = pio_inbyte( CB_SN );
   if ( ( sc == 0x55 ) && ( sn == 0xaa ) )
      reg_config_info[0] = REG_CONFIG_TYPE_UNKN;

   // lets see if there is a device 1

   pio_outbyte( CB_DH, CB_DH_DEV1 );
   DELAY400NS;
   pio_outbyte( CB_SC, 0x55 );
   pio_outbyte( CB_SN, 0xaa );
   pio_outbyte( CB_SC, 0xaa );
   pio_outbyte( CB_SN, 0x55 );
   pio_outbyte( CB_SC, 0x55 );
   pio_outbyte( CB_SN, 0xaa );
   sc = pio_inbyte( CB_SC );
   sn = pio_inbyte( CB_SN );
   if ( ( sc == 0x55 ) && ( sn == 0xaa ) )
      reg_config_info[1] = REG_CONFIG_TYPE_UNKN;

   // now we think we know which devices, if any are there,
   // so lets try a soft reset (ignoring any errors).

   pio_outbyte( CB_DH, CB_DH_DEV0 );
   DELAY400NS;
   reg_reset( 0 );

   // lets check device 0 again, is device 0 really there?
   // is it ATA or ATAPI?

   pio_outbyte( CB_DH, CB_DH_DEV0 );
   DELAY400NS;
   sc = pio_inbyte( CB_SC );
   sn = pio_inbyte( CB_SN );
   if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
   {
      reg_config_info[0] = REG_CONFIG_TYPE_UNKN;
      st = pio_inbyte( CB_STAT );
      cl = pio_inbyte( CB_CL );
      ch = pio_inbyte( CB_CH );
      if ( ( ( cl == 0x14 ) && ( ch == 0xeb ) )       // PATAPI
           ||
           ( ( cl == 0x69 ) && ( ch == 0x96 ) )       // SATAPI
         )
      {
         reg_config_info[0] = REG_CONFIG_TYPE_ATAPI;
      }
      else
      if ( ( st != 0 )
           &&
           ( ( ( cl == 0x00 ) && ( ch == 0x00 ) )     // PATA
             ||
             ( ( cl == 0x3c ) && ( ch == 0xc3 ) ) )   // SATA
         )
      {
         reg_config_info[0] = REG_CONFIG_TYPE_ATA;
      }
   }

   // lets check device 1 again, is device 1 really there?
   // is it ATA or ATAPI?

   pio_outbyte( CB_DH, CB_DH_DEV1 );
   DELAY400NS;
   sc = pio_inbyte( CB_SC );
   sn = pio_inbyte( CB_SN );
   if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
   {
      reg_config_info[1] = REG_CONFIG_TYPE_UNKN;
      st = pio_inbyte( CB_STAT );
      cl = pio_inbyte( CB_CL );
      ch = pio_inbyte( CB_CH );
      if ( ( ( cl == 0x14 ) && ( ch == 0xeb ) )       // PATAPI
           ||
           ( ( cl == 0x69 ) && ( ch == 0x96 ) )       // SATAPI
         )
      {
         reg_config_info[1] = REG_CONFIG_TYPE_ATAPI;
      }
      else
      if ( ( st != 0 )
           &&
           ( ( ( cl == 0x00 ) && ( ch == 0x00 ) )     // PATA
             ||
             ( ( cl == 0x3c ) && ( ch == 0xc3 ) ) )   // SATA
         )
      {
         reg_config_info[1] = REG_CONFIG_TYPE_ATA;
      }
   }

   // If possible, select a device that exists, try device 0 first.

   if ( reg_config_info[1] != REG_CONFIG_TYPE_NONE )
   {
      pio_outbyte( CB_DH, CB_DH_DEV1 );
      DELAY400NS;
      numDev ++ ;
   }
   if ( reg_config_info[0] != REG_CONFIG_TYPE_NONE )
   {
      pio_outbyte( CB_DH, CB_DH_DEV0 );
      DELAY400NS;
      numDev ++ ;
   }

   // BMIDE Error=1?

   if ( pio_readBusMstrStatus() & BM_SR_MASK_ERR )
   {
      reg_cmd_info.ec = 78;                  // yes
   }

   // return the number of devices found

   return numDev;
}

//*************************************************************
//
// reg_reset() - Execute a Software Reset.
//
// See ATA-2 Section 9.2, ATA-3 Section 9.2, ATA-4 Section 8.3.
//
//*************************************************************

int reg_reset( unsigned char devRtrn )

{
   unsigned char sc;
   unsigned char sn;
   unsigned char status;
   unsigned char dc;

   // setup register values

   dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );

   // reset Bus Master Error bit

   pio_writeBusMstrStatus( BM_SR_MASK_ERR );

   // initialize the command timeout counter

   tmr_set_timeout();

   // Set and then reset the soft reset bit in the Device Control
   // register.  This causes device 0 be selected.

   pio_outbyte( CB_DC, (unsigned char) ( dc | CB_DC_SRST ) );
   DELAY400NS;
   pio_outbyte( CB_DC, dc );
   DELAY400NS;

   // If there is a device 0, wait for device 0 to set BSY=0.

   if ( reg_config_info[0] != REG_CONFIG_TYPE_NONE )
   {
      while ( 1 )
      {
         status = pio_inbyte( CB_STAT );
         if ( ( status & CB_STAT_BSY ) == 0 )
            break;
         if ( tmr_chk_timeout() )
         {
            reg_cmd_info.to = 1;
            reg_cmd_info.ec = 1;
            break;
         }
      }
   }

   // If there is a device 1, wait until device 1 allows
   // register access.

   if ( reg_config_info[1] != REG_CONFIG_TYPE_NONE )
   {
      while ( 1 )
      {
         pio_outbyte( CB_DH, CB_DH_DEV1 );
         DELAY400NS;
         sc = pio_inbyte( CB_SC );
         sn = pio_inbyte( CB_SN );
         if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
            break;
         if ( tmr_chk_timeout() )
         {
            reg_cmd_info.to = 1;
            reg_cmd_info.ec = 2;
            break;
         }
      }

      // Now check if drive 1 set BSY=0.

      if ( reg_cmd_info.ec == 0 )
      {
         if ( pio_inbyte( CB_STAT ) & CB_STAT_BSY )
         {
            reg_cmd_info.ec = 3;
         }
      }
   }

   // RESET_DONE:

   // We are done but now we must select the device the caller
   // requested. This will cause
   // the correct data to be returned in reg_cmd_info.

   pio_outbyte( CB_DH, (unsigned char) ( devRtrn ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   DELAY400NS;

   // If possible, select a device that exists,
   // try device 0 first.

   if ( reg_config_info[1] != REG_CONFIG_TYPE_NONE )
   {
      pio_outbyte( CB_DH, CB_DH_DEV1 );
      DELAY400NS;
   }
   if ( reg_config_info[0] != REG_CONFIG_TYPE_NONE )
   {
      pio_outbyte( CB_DH, CB_DH_DEV0 );
      DELAY400NS;
   }

   // BMIDE Error=1?

   if ( pio_readBusMstrStatus() & BM_SR_MASK_ERR )
   {
      reg_cmd_info.ec = 78;                  // yes
   }

   // All done.  The return values of this function are described in
   // MINDRVR.H.

   sub_trace_command();
   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

//*************************************************************
//
// exec_non_data_cmd() - Execute a non-data command.
//
// This includes the strange ATAPI DEVICE RESET 'command'
// (command code 08H).
//
// Note special handling for Execute Device Diagnostics
// command when there is no device 0.
//
// See ATA-2 Section 9.5, ATA-3 Section 9.5,
// ATA-4 Section 8.8 Figure 12.  Also see Section 8.5.
//
//*************************************************************

static int exec_non_data_cmd( unsigned char dev );

static int exec_non_data_cmd( unsigned char dev )

{
   unsigned char secCnt;
   unsigned char secNum;
   unsigned char status;
   int polled = 0;

   // reset Bus Master Error bit

   pio_writeBusMstrStatus( BM_SR_MASK_ERR );

   // Set command time out.

   tmr_set_timeout();

   // PAY ATTENTION HERE
   // If the caller is attempting a Device Reset command, then
   // don't do most of the normal stuff.  Device Reset has no
   // parameters, should not generate an interrupt and it is the
   // only command that can be written to the Command register
   // when a device has BSY=1 (a very strange command!).  Not
   // all devices support this command (even some ATAPI devices
   // don't support the command.

   if ( reg_cmd_info.cmd != CMD_DEVICE_RESET )
   {
      // Select the drive - call the sub_select function.
      // Quit now if this fails.

      if ( sub_select( dev ) )
      {
         return 1;
      }

      // Set up all the registers except the command register.

      sub_setup_command();
   }

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.

   pio_outbyte( CB_CMD, reg_cmd_info.cmd );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not even started the
   // command yet.

   DELAY400NS;

   // IF
   //    This is an Exec Dev Diag command (cmd=0x90)
   //    and there is no device 0 then
   //    there will be no interrupt. So we must
   //    poll device 1 until it allows register
   //    access and then do normal polling of the Status
   //    register for BSY=0.
   // ELSE
   // IF
   //    This is a Dev Reset command (cmd=0x08) then
   //    there should be no interrupt.  So we must
   //    poll for BSY=0.
   // ELSE
   //    Do the normal wait for interrupt or polling for
   //    completion.

   if ( ( reg_cmd_info.cmd == CMD_EXECUTE_DEVICE_DIAGNOSTIC )
        &&
        ( reg_config_info[0] == REG_CONFIG_TYPE_NONE )
      )
   {
      polled = 1;
      while ( 1 )
      {
         pio_outbyte( CB_DH, CB_DH_DEV1 );
         DELAY400NS;
         secCnt = pio_inbyte( CB_SC );
         secNum = pio_inbyte( CB_SN );
         if ( ( secCnt == 0x01 ) && ( secNum == 0x01 ) )
            break;
         if ( tmr_chk_timeout() )
         {
            reg_cmd_info.to = 1;
            reg_cmd_info.ec = 24;
            break;
         }
      }
   }
   else
   {
      if ( reg_cmd_info.cmd == CMD_DEVICE_RESET )
      {
         // Wait for not BUSY -or- wait for time out.

         polled = 1;
         sub_wait_poll( 0, 23 );
      }
      else
      {
         // Wait for interrupt -or- wait for not BUSY -or- wait for time out.

         if ( ! int_use_intr_flag )
            polled = 1;
         sub_wait_poll( 22, 23 );
      }
   }

   // If status was polled or if any error read the status register,
   // otherwise get the status that was read by the interrupt handler.

   if ( ( polled ) || ( reg_cmd_info.ec ) )
      status = pio_inbyte( CB_STAT );
   else
      status = int_ata_status;

   // Error if BUSY, DEVICE FAULT, DRQ or ERROR status now.

   if ( reg_cmd_info.ec == 0 )
   {
      if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         reg_cmd_info.ec = 21;
      }
   }

   // BMIDE Error=1?

   if ( pio_readBusMstrStatus() & BM_SR_MASK_ERR )
   {
      reg_cmd_info.ec = 78;                  // yes
   }

   // NON_DATA_DONE:

   // All done.  The return values of this function are described in
   // MINDRVR.H.

   sub_trace_command();
   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

//*************************************************************
//
// reg_non_data_lba28() - Easy way to execute a non-data command
//                        using an LBA sector address.
//
//*************************************************************

int reg_non_data_lba28( unsigned char dev, unsigned char cmd,
                        unsigned int fr, unsigned int sc,
                        unsigned long lba )

{

   // Setup current command information.

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );
   reg_cmd_info.ns  = sc;
   reg_cmd_info.lbaSize = LBA28;
   reg_cmd_info.lbaHigh = 0L;
   reg_cmd_info.lbaLow = lba;

   // Execute the command.

   return exec_non_data_cmd( dev );
}

//*************************************************************
//
// reg_non_data_lba48() - Easy way to execute a non-data command
//                        using an LBA sector address.
//
//*************************************************************

int reg_non_data_lba48( unsigned char dev, unsigned char cmd,
                        unsigned int fr, unsigned int sc,
                        unsigned long lbahi, unsigned long lbalo )

{

   // Setup current command infomation.

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );
   reg_cmd_info.ns  = sc;
   reg_cmd_info.lbaSize = LBA48;
   reg_cmd_info.lbaHigh = lbahi;
   reg_cmd_info.lbaLow = lbalo;

   // Execute the command.

   return exec_non_data_cmd( dev );
}

//*************************************************************
//
// exec_pio_data_in_cmd() - Execute a PIO Data In command.
//
// See ATA-2 Section 9.3, ATA-3 Section 9.3,
// ATA-4 Section 8.6 Figure 10.
//
//*************************************************************

static int exec_pio_data_in_cmd( unsigned char dev,
                            unsigned char * bufAddr,
                            long numSect, int multiCnt );


static int exec_pio_data_in_cmd( unsigned char dev,
                            unsigned char * bufAddr,
                            long numSect, int multiCnt )

{
   unsigned char status;
   long wordCnt;

   // reset Bus Master Error bit

   pio_writeBusMstrStatus( BM_SR_MASK_ERR );

   // Set command time out.

   tmr_set_timeout();

   // Select the drive - call the sub_select function.
   // Quit now if this fails.

   if ( sub_select( dev ) )
   {
      return 1;
   }

   // Set up all the registers except the command register.

   sub_setup_command();

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.

   pio_outbyte( CB_CMD, reg_cmd_info.cmd );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not even started the
   // command yet.

   DELAY400NS;

   // Loop to read each sector.

   while ( 1 )
   {
      // READ_LOOP:
      //
      // NOTE NOTE NOTE ...  The primary status register (1f7) MUST NOT be
      // read more than ONCE for each sector transferred!  When the
      // primary status register is read, the drive resets IRQ.  The
      // alternate status register (3f6) can be read any number of times.
      // After interrupt read the the primary status register ONCE
      // and transfer the 256 words (REP INSW).  AS SOON as BOTH the
      // primary status register has been read AND the last of the 256
      // words has been read, the drive is allowed to generate the next
      // IRQ (newer and faster drives could generate the next IRQ in
      // 50 microseconds or less).  If the primary status register is read
      // more than once, there is the possibility of a race between the
      // drive and the software and the next IRQ could be reset before
      // the system interrupt controller sees it.

      // Wait for interrupt -or- wait for not BUSY -or- wait for time out.

      sub_wait_poll( 34, 35 );

      // If polling or error read the status, otherwise
      // get the status that was read by the interrupt handler.

      if ( ( ! int_use_intr_flag ) || ( reg_cmd_info.ec ) )
         status = pio_inbyte( CB_STAT );
      else
         status = int_ata_status;

      // If there was a time out error, go to READ_DONE.

      if ( reg_cmd_info.ec )
         break;   // go to READ_DONE

      // If BSY=0 and DRQ=1, transfer the data,
      // even if we find out there is an error later.

      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == CB_STAT_DRQ )
      {

         // increment number of DRQ packets

         reg_cmd_info.drqPackets ++ ;

         // determine the number of sectors to transfer

         wordCnt = multiCnt ? multiCnt : 1;
         if ( wordCnt > numSect )
            wordCnt = numSect;
         wordCnt = wordCnt * 256;

         // Do the REP INSW to read the data for one DRQ block.

         reg_cmd_info.totalBytesXfer += ( wordCnt << 1 );
         pio_drq_block_in( CB_DATA, bufAddr, wordCnt );

         DELAY400NS;    // delay so device can get the status updated

         // Note: The drive should have dropped DATA REQUEST by now.  If there
         // are more sectors to transfer, BUSY should be active now (unless
         // there is an error).

         // Decrement the count of sectors to be transferred
         // and increment buffer address.

         numSect = numSect - ( multiCnt ? multiCnt : 1 );
         bufAddr = bufAddr + ( 512 * ( multiCnt ? multiCnt : 1 ) );
      }

      // So was there any error condition?

      if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_ERR ) )
      {
         reg_cmd_info.ec = 31;
         break;   // go to READ_DONE
      }

      // DRQ should have been set -- was it?

      if ( ( status & CB_STAT_DRQ ) == 0 )
      {
         reg_cmd_info.ec = 32;
         break;   // go to READ_DONE
      }

      // If all of the requested sectors have been transferred, make a
      // few more checks before we exit.

      if ( numSect < 1 )
      {
         // Since the drive has transferred all of the requested sectors
         // without error, the drive should not have BUSY, DEVICE FAULT,
         // DATA REQUEST or ERROR active now.
		do {
			status = pio_inbyte( CB_STAT );
         }
         while (status & CB_STAT_BSY);

         if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
         {
            reg_cmd_info.ec = 33;
            break;   // go to READ_DONE
         }

         // All sectors have been read without error, go to READ_DONE.

         break;   // go to READ_DONE

      }

      // This is the end of the read loop.  If we get here, the loop is
      // repeated to read the next sector.  Go back to READ_LOOP.

   }

   // BMIDE Error=1?

   if ( pio_readBusMstrStatus() & BM_SR_MASK_ERR )
   {
      reg_cmd_info.ec = 78;                  // yes
   }

   // READ_DONE:

   // All done.  The return values of this function are described in
   // MINDRVR.H.

   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

//*************************************************************
//
// reg_pio_data_in_lba28() - Easy way to execute a PIO Data In command
//                           using an LBA sector address.
//
//*************************************************************

int reg_pio_data_in_lba28( unsigned char dev, unsigned char cmd,
                           unsigned int fr, unsigned int sc,
                           unsigned long lba,
                           unsigned char * bufAddr,
                           long numSect, int multiCnt )

{

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );
   reg_cmd_info.lbaSize = LBA28;
   reg_cmd_info.lbaHigh = 0L;
   reg_cmd_info.lbaLow = lba;
   // these commands transfer only 1 sector
   if (    ( cmd == CMD_IDENTIFY_DEVICE )
        || ( cmd == CMD_IDENTIFY_DEVICE_PACKET )
      )
      numSect = 1;

   // adjust multiple count
   if ( multiCnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multiCnt &= 0x00ff;
   }
   else
   {
      // only Read Multiple uses multiCnt
      if ( cmd != CMD_READ_MULTIPLE )
         multiCnt = 1;
   }

   reg_cmd_info.ns  = numSect;
   reg_cmd_info.mc  = multiCnt;

   return exec_pio_data_in_cmd( dev, bufAddr, numSect, multiCnt );
}

//*************************************************************
//
// reg_pio_data_in_lba48() - Easy way to execute a PIO Data In command
//                           using an LBA sector address.
//
//*************************************************************

int reg_pio_data_in_lba48( unsigned char dev, unsigned char cmd,
                           unsigned int fr, unsigned int sc,
                           unsigned long lbahi, unsigned long lbalo,
                           unsigned char * bufAddr,
                           long numSect, int multiCnt )

{

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );
   reg_cmd_info.lbaSize = LBA48;
   reg_cmd_info.lbaHigh = lbahi;
   reg_cmd_info.lbaLow = lbalo;

   // adjust multiple count
   if ( multiCnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multiCnt &= 0x00ff;
   }
   else
   {
      // only Read Multiple Ext uses multiCnt
      if ( cmd != CMD_READ_MULTIPLE_EXT )
         multiCnt = 1;
   }

   reg_cmd_info.ns  = numSect;
   reg_cmd_info.mc  = multiCnt;

   return exec_pio_data_in_cmd( dev, bufAddr, numSect, multiCnt );
}

//*************************************************************
//
// exec_pio_data_out_cmd() - Execute a PIO Data Out command.
//
// See ATA-2 Section 9.4, ATA-3 Section 9.4,
// ATA-4 Section 8.7 Figure 11.
//
//*************************************************************

static int exec_pio_data_out_cmd( unsigned char dev,
                             unsigned char * bufAddr,
                             long numSect, int multiCnt );

static int exec_pio_data_out_cmd( unsigned char dev,
                             unsigned char * bufAddr,
                             long numSect, int multiCnt )

{
   unsigned char status;
   int loopFlag = 1;
   long wordCnt;

   // reset Bus Master Error bit

   pio_writeBusMstrStatus( BM_SR_MASK_ERR );

   // Set command time out.

   tmr_set_timeout();

   // Select the drive - call the sub_select function.
   // Quit now if this fails.

   if ( sub_select( dev ) )
   {
      return 1;
   }

   // Set up all the registers except the command register.

   sub_setup_command();

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.

   pio_outbyte( CB_CMD, reg_cmd_info.cmd );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not even started the
   // command yet.

   DELAY400NS;

   // Wait for not BUSY or time out.
   // Note: No interrupt is generated for the
   // first sector of a write command.

   while ( 1 )
   {
      status = pio_inbyte( CB_ASTAT );
      if ( ( status & CB_STAT_BSY ) == 0 )
         break;
      if ( tmr_chk_timeout() )
      {
         reg_cmd_info.to = 1;
         reg_cmd_info.ec = 47;
         loopFlag = 0;
         break;
      }
   }

   // This loop writes each sector.

   while ( loopFlag )
   {
      // WRITE_LOOP:
      //
      // NOTE NOTE NOTE ...  The primary status register (1f7) MUST NOT be
      // read more than ONCE for each sector transferred!  When the
      // primary status register is read, the drive resets IRQ.  The
      // alternate status register (3f6) can be read any number of times.
      // For correct results, transfer the 256 words (REP OUTSW), wait for
      // interrupt and then read the primary status register.  AS
      // SOON as BOTH the primary status register has been read AND the
      // last of the 256 words has been written, the drive is allowed to
      // generate the next IRQ (newer and faster drives could generate
      // the next IRQ in 50 microseconds or less).  If the primary
      // status register is read more than once, there is the possibility
      // of a race between the drive and the software and the next IRQ
      // could be reset before the system interrupt controller sees it.

      // If BSY=0 and DRQ=1, transfer the data,
      // even if we find out there is an error later.

      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == CB_STAT_DRQ )
      {

         // increment number of DRQ packets

         reg_cmd_info.drqPackets ++ ;

         // determine the number of sectors to transfer

         wordCnt = multiCnt ? multiCnt : 1;
         if ( wordCnt > numSect )
            wordCnt = numSect;
         wordCnt = wordCnt * 256;

         // Do the REP OUTSW to write the data for one DRQ block.

         reg_cmd_info.totalBytesXfer += ( wordCnt << 1 );
         pio_drq_block_out( CB_DATA, bufAddr, wordCnt );

         DELAY400NS;    // delay so device can get the status updated

         // Note: The drive should have dropped DATA REQUEST and
         // raised BUSY by now.

         // Decrement the count of sectors to be transferred
         // and increment buffer address.

         numSect = numSect - ( multiCnt ? multiCnt : 1 );
         bufAddr = bufAddr + ( 512 * ( multiCnt ? multiCnt : 1 ) );
      }

      // So was there any error condition?

      if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_ERR ) )
      {
         reg_cmd_info.ec = 41;
         break;   // go to WRITE_DONE
      }

      // DRQ should have been set -- was it?

      if ( ( status & CB_STAT_DRQ ) == 0 )
      {
         reg_cmd_info.ec = 42;
         break;   // go to WRITE_DONE
      }

      // Wait for interrupt -or- wait for not BUSY -or- wait for time out.

      sub_wait_poll( 44, 45 );

      // If polling or error read the status, otherwise
      // get the status that was read by the interrupt handler.

      if ( ( ! int_use_intr_flag ) || ( reg_cmd_info.ec ) )
         status = pio_inbyte( CB_STAT );
      else
         status = int_ata_status;

      // If there was a time out error, go to WRITE_DONE.

      if ( reg_cmd_info.ec )
         break;   // go to WRITE_DONE

      // If all of the requested sectors have been transferred, make a
      // few more checks before we exit.

      if ( numSect < 1 )
      {
         // Since the drive has transferred all of the sectors without
         // error, the drive MUST not have BUSY, DEVICE FAULT, DATA REQUEST
         // or ERROR status at this time.

         if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
         {
            reg_cmd_info.ec = 43;
            break;   // go to WRITE_DONE
         }

         // All sectors have been written without error, go to WRITE_DONE.

         break;   // go to WRITE_DONE

      }

      //
      // This is the end of the write loop.  If we get here, the loop
      // is repeated to write the next sector.  Go back to WRITE_LOOP.

   }

   // BMIDE Error=1?

   if ( pio_readBusMstrStatus() & BM_SR_MASK_ERR )
   {
      reg_cmd_info.ec = 78;                  // yes
   }

   // WRITE_DONE:

   // All done.  The return values of this function are described in
   // MINDRVR.H.

   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

//*************************************************************
//
// reg_pio_data_out_lba28() - Easy way to execute a PIO Data In command
//                            using an LBA sector address.
//
//*************************************************************

int reg_pio_data_out_lba28( unsigned char dev, unsigned char cmd,
                            unsigned int fr, unsigned int sc,
                            unsigned long lba,
                            unsigned char * bufAddr,
                            long numSect, int multiCnt )

{

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );
   reg_cmd_info.lbaSize = LBA28;
   reg_cmd_info.lbaHigh = 0;
   reg_cmd_info.lbaLow = lba;

   // adjust multiple count
   if ( multiCnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multiCnt &= 0x00ff;
   }
   else
   {
      // only Write Multiple and CFA Write Multiple W/O Erase uses multiCnt
      if (    ( cmd != CMD_WRITE_MULTIPLE )
           && ( cmd != CMD_CFA_WRITE_MULTIPLE_WO_ERASE )
         )
         multiCnt = 1;
   }

   reg_cmd_info.ns  = numSect;
   reg_cmd_info.mc  = multiCnt;

   return exec_pio_data_out_cmd( dev, bufAddr, numSect, multiCnt );
}

//*************************************************************
//
// reg_pio_data_out_lba48() - Easy way to execute a PIO Data In command
//                            using an LBA sector address.
//
//*************************************************************

int reg_pio_data_out_lba48( unsigned char dev, unsigned char cmd,
                            unsigned int fr, unsigned int sc,
                            unsigned long lbahi, unsigned long lbalo,
                            unsigned char * bufAddr,
                            long numSect, int multiCnt )

{

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );
   reg_cmd_info.lbaSize = LBA48;
   reg_cmd_info.lbaHigh = lbahi;
   reg_cmd_info.lbaLow = lbalo;

   // adjust multiple count
   if ( multiCnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multiCnt &= 0x00ff;
   }
   else
   {
      // only Write Multiple Ext uses multiCnt
      if ( cmd != CMD_WRITE_MULTIPLE_EXT )
         multiCnt = 1;
   }

   reg_cmd_info.ns  = numSect;
   reg_cmd_info.mc  = multiCnt;

   return exec_pio_data_out_cmd( dev, bufAddr, numSect, multiCnt );
}

#if INCLUDE_ATAPI_PIO

//*************************************************************
//
// reg_packet() - Execute an ATAPI Packet (A0H) command.
//
// See ATA-4 Section 9.10, Figure 14.
//
//*************************************************************

int reg_packet( unsigned char dev,
                unsigned int cpbc,
                unsigned char * cdbBufAddr,
                int dir,
                long dpbc,
                unsigned char * dataBufAddr )

{
   unsigned char status;
   unsigned int byteCnt;
   long wordCnt;

   // reset Bus Master Error bit

   pio_writeBusMstrStatus( BM_SR_MASK_ERR );

   // Make sure the command packet size is either 12 or 16
   // and save the command packet size and data.

   cpbc = cpbc < 12 ? 12 : cpbc;
   cpbc = cpbc > 12 ? 16 : cpbc;

   // Setup current command information.

   reg_cmd_info.ec = 0;
   reg_cmd_info.cmd = CMD_PACKET;
   reg_cmd_info.fr = 0;
   reg_cmd_info.sc = 0;
   reg_cmd_info.sn = 0;
   reg_cmd_info.cl = (unsigned char) ( dpbc & 0x00ff );
   reg_cmd_info.ch = ( unsigned char) ( ( dpbc & 0xff00 ) >> 8 );
   reg_cmd_info.dh = (unsigned char) ( dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   reg_cmd_info.dc = (unsigned char) ( int_use_intr_flag ? 0 : CB_DC_NIEN );
   reg_cmd_info.lbaSize = 0;
   // Set command time out.

   tmr_set_timeout();

   // Select the drive - call the sub_select function.
   // Quit now if this fails.

   if ( sub_select( dev ) )
   {
      return 1;
   }

   // Set up all the registers except the command register.

   sub_setup_command();

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.

   pio_outbyte( CB_CMD, CMD_PACKET );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not even started the
   // command yet.

   DELAY400NS;

   // Command packet transfer...
   // Poll Alternate Status for BSY=0.

   while ( 1 )
   {
      status = pio_inbyte( CB_ASTAT );       // poll for not busy
      if ( ( status & CB_STAT_BSY ) == 0 )
         break;
      if ( tmr_chk_timeout() )               // time out yet ?
      {
         reg_cmd_info.to = 1;
         reg_cmd_info.ec = 51;
         dir = -1;   // command done
         break;
      }
   }

   // Command packet transfer...
   // Check for protocol failures... no interrupt here please!

   // Command packet transfer...
   // If no error, transfer the command packet.

   if ( reg_cmd_info.ec == 0 )
   {

      // Command packet transfer...
      // Read the primary status register and the other ATAPI registers.

      status = pio_inbyte( CB_STAT );

      // Command packet transfer...
      // check status: must have BSY=0, DRQ=1 now

      if (    ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) )
           != CB_STAT_DRQ
         )
      {
         reg_cmd_info.ec = 52;
         dir = -1;   // command done
      }
      else
      {
         // Command packet transfer...
         // xfer the command packet (the cdb)

         pio_drq_block_out( CB_DATA, cdbBufAddr, cpbc >> 1 );

         DELAY400NS;    // delay so device can get the status updated
      }
   }

   // Data transfer loop...
   // If there is no error, enter the data transfer loop.

   while ( reg_cmd_info.ec == 0 )
   {
      // Data transfer loop...
      // Wait for interrupt -or- wait for not BUSY -or- wait for time out.

      sub_wait_poll( 53, 54 );

      // Data transfer loop...
      // If there was a time out error, exit the data transfer loop.

      if ( reg_cmd_info.ec )
      {
         dir = -1;   // command done
         break;
      }

      // Data transfer loop...
      // If using interrupts get the status read by the interrupt
      // handler, otherwise read the status register.

      if ( int_use_intr_flag )
         status = int_ata_status;
      else
         status = pio_inbyte( CB_STAT );

      // Data transfer loop...
      // Exit the read data loop if the device indicates this
      // is the end of the command.

      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == 0 )
      {
         dir = -1;   // command done
         break;
      }

      // Data transfer loop...
      // The device must want to transfer data...
      // check status: must have BSY=0, DRQ=1 now.

      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) != CB_STAT_DRQ )
      {
         reg_cmd_info.ec = 55;
         dir = -1;   // command done
         break;
      }

      // Data transfer loop...
      // get the byte count, check for zero...

      byteCnt = ( pio_inbyte( CB_CH ) << 8 ) | pio_inbyte( CB_CL );
      if ( byteCnt < 1 )
      {
         reg_cmd_info.ec = 59;
         dir = -1;   // command done
         break;
      }

      // Data transfer loop...
      // increment number of DRQ packets

      reg_cmd_info.drqPackets ++ ;

      // Data transfer loop...
      // transfer the data and update the i/o buffer address
      // and the number of bytes transfered.

      wordCnt = ( byteCnt >> 1 ) + ( byteCnt & 0x0001 );
      reg_cmd_info.totalBytesXfer += ( wordCnt << 1 );
      if ( dir )
         pio_drq_block_out( CB_DATA, dataBufAddr, wordCnt );
      else
         pio_drq_block_in( CB_DATA, dataBufAddr, wordCnt );
      dataBufAddr = dataBufAddr + byteCnt;

      DELAY400NS;    // delay so device can get the status updated
   }

   // End of command...
   // Wait for interrupt or poll for BSY=0,
   // but don't do this if there was any error or if this
   // was a commmand that did not transfer data.

   if ( ( reg_cmd_info.ec == 0 ) && ( dir >= 0 ) )
   {
      sub_wait_poll( 56, 57 );
   }

   // Final status check, only if no previous error.

   if ( reg_cmd_info.ec == 0 )
   {
      // Final status check...
      // If using interrupts get the status read by the interrupt
      // handler, otherwise read the status register.

      if ( int_use_intr_flag )
         status = int_ata_status;
      else
         status = pio_inbyte( CB_STAT );

      // Final status check...
      // check for any error.

      if ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         reg_cmd_info.ec = 58;
      }
   }

   // Done...

   // Final status check
   // BMIDE Error=1?

   if ( pio_readBusMstrStatus() & BM_SR_MASK_ERR )
   {
      reg_cmd_info.ec = 78;                  // yes
   }

   // All done.  The return values of this function are described in
   // MINDRVR.H.

   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

#endif   // INCLUDE_ATAPI

#if INCLUDE_ATA_DMA

//***********************************************************
//
// Some notes about PCI bus mastering DMA...
//
// !!! The DMA support in MINDRVR is based on x86 PCI bus mastering
// !!! ATA controller design as described by the T13 document
// !!! '1510 Host Controller Standard' (in sections 1-6).
//
// Note that the T13 1510D document also describes a
// complex DMA engine called ADMA.  While ADMA is a good idea it
// will probably never be popular or widely implemented.  MINDRVR
// does not support ADMA.
//
// The base address of the Bus Master Control Registers (BMIDE) is
// found in the PCI Configuration space for the ATA controller (at
// offset 0x20 in the config space data).  This is normally an I/O
// address.
//
// The BMIDE data is 16 bytes of data starting at the BMIDE base
// address.  The first 8 bytes is for the primary ATA channel and
// the second 8 bytes is for the secondary ATA channel.  The 8 bytes
// contain a "command" byte and a "status" byte and a 4 byte
// (32-bit) physical memory address pointing to the Physical Region
// Descriptor (PRD) list.  Each PRD entry describes an area of
// memory or data buffer for the DMA transfer.  A region described
// by a PRD may not cross a 64K byte boundary in physical memory.
// Also, the PRD list must not cross a 64K byte boundary.
//
//***********************************************************

//***********************************************************
//
// pci bus master registers and PRD list buffer,
// see the dma_pci_config() and set_up_xfer() functions.
//
// !!! Note that the PRD buffer is statically allocated here
// !!! but the actual address of the buffer is adjusted by
// !!! the dma_pci_config() function.
//
//***********************************************************

unsigned long * dma_pci_prd_ptr;   // current PRD buffer address
int dma_pci_num_prd;               // current number of PRD entries

unsigned char statReg;             // save BM status reg bits
static unsigned char rwControl;           // read/write control bit setting

unsigned char *prdBuf;                  // PRD buffer
unsigned long *prdBufPtr;               // first PRD addr

//***********************************************************
//
// dma_pci_config() - configure/setup for Read/Write DMA
//
// The caller must call this function before attempting
// to use any ATA or ATAPI commands in PCI DMA mode.
//
// !!! MINDRVR assumes the entire DMA data transfer is contained
// !!! within a single contiguous I/O buffer. You may not need
// !!! the dma_pci_config() function depending on how your system
// !!! allocates the PRD buffer.
//
// !!! This function shows an example of PRD buffer allocation.
// !!! The PRD buffer must be aligned
// !!! on a 8 byte boundary and must not cross a 64K byte
// !!! boundary in memory.
//
//***********************************************************
/*
int dma_pci_config( void )

{
   unsigned long lw;

   // Set up the PRD entry list buffer address - the PRD entry list
   // may not span a 64KB boundary in physical memory. Space is
   // allocated (above) for this buffer such that it will be
   // aligned on a seqment boundary
   // and such that the PRD list will not span a 64KB boundary...
   lw = (unsigned long) prdBuf;
   // ...move up to an 8 byte boundary.
   lw = lw + 15;
   lw = lw & 0xfffffff8L;
   // ...check for 64KB boundary in the first part of the PRD buffer,
   // ...if so just move the buffer to that boundary.
   if ( ( lw & 0xffff0000L )
        !=
        ( ( lw + ( MAX_PRD * 8L ) - 1L ) & 0xffff0000L )
      )
      lw = ( lw + ( MAX_PRD * 8L ) ) & 0xffff0000L;
   // ... return the address of the first PRD
   dma_pci_prd_ptr = prdBufPtr = (unsigned long *) lw;
   // ... return the current number of PRD entries
   dma_pci_num_prd = 0;

   // read the BM status reg and save the upper 3 bits.
   statReg = (unsigned char) ( pio_readBusMstrStatus() & 0x60 );

   return 0;
}
*/
//***********************************************************
//
// set_up_xfer() -- set up the PRD entry list
//
// !!! MINDRVR assumes the entire DMA data transfer is contained
// !!! within a single contiguous I/O buffer. You may not need
// !!! a much more complex set_up_xfer() function to support
// !!! true scatter/gather lists.
//
// The PRD list must be aligned on an 8 byte boundary and the
// list must not cross a 64K byte boundary in memory.
//
//***********************************************************

static int set_up_xfer( int dir, long bc, unsigned char * bufAddr );

static int set_up_xfer( int dir, long bc, unsigned char * bufAddr )

{
   int numPrd;                      // number of PRD required
   int maxPrd;                      // max number of PRD allowed
   unsigned long temp;
   unsigned long phyAddr;           // physical memory address
   unsigned long * prdPtr;      // pointer to PRD entry list

   // disable/stop the dma channel, clear interrupt and error bits
   pio_writeBusMstrCmd( BM_CR_MASK_STOP );
   pio_writeBusMstrStatus( (unsigned char) ( statReg | BM_SR_MASK_INT | BM_SR_MASK_ERR ) );

   // setup to build the PRD list...
   // ...max PRDs allowed
   maxPrd = (int) MAX_PRD;
   // ...PRD buffer address
   prdPtr = prdBufPtr;
   dma_pci_prd_ptr = prdPtr;
   // ... convert I/O buffer address to an physical memory address
   phyAddr = (unsigned long) bufAddr;

   // build the PRD list...
   // ...PRD entry format:
   // +0 to +3 = memory address
   // +4 to +5 = 0x0000 (not EOT) or 0x8000 (EOT)
   // +6 to +7 = byte count
   // ...zero number of PRDs
   numPrd = 0;
   // ...loop to build each PRD
   while ( bc > 0 )
   {
      if ( numPrd >= maxPrd )
         return 1;
      // set this PRD's address
      prdPtr[0] = phyAddr;
      // set count for this PRD
      temp = 65536L;          // max PRD length
      if ( temp > bc )        // count to large?
         temp = bc;           //    yes - use actual count
      // check if count will fit
      phyAddr = phyAddr + temp;
      if ( ( phyAddr & 0xffff0000L ) != ( prdPtr[0] & 0xffff0000L ) )
      {
         phyAddr = phyAddr & 0xffff0000L;
         temp = phyAddr - prdPtr[0];
      }
      // set this PRD's count
      prdPtr[1] = temp & 0x0000ffffL;
      // update byte count
      bc = bc - temp;
      // set the end bit in the prd list
      if ( bc < 1 )
         prdPtr[1] = prdPtr[1] | 0x80000000L;
      prdPtr ++ ;
      prdPtr ++ ;
      numPrd ++ ;
   }

   // return the current PRD list size and
   // write into BMIDE PRD address registers.

   dma_pci_num_prd = numPrd;
   /*
   * (unsigned long *) (pio_bmide_base_addr + BM_PRD_ADDR_LOW )
      = (unsigned long) prdBufPtr;
   */
   pio_writeBusMstrPrd((unsigned long)prdBufPtr);
   // set the read/write control:
   // PCI reads for ATA Write DMA commands,
   // PCI writes for ATA Read DMA commands.

   if ( dir )
      rwControl = BM_CR_MASK_READ;     // ATA Write DMA
   else
      rwControl = BM_CR_MASK_WRITE;    // ATA Read DMA
   pio_writeBusMstrCmd( rwControl );
   return 0;
}

//***********************************************************
//
// exec_pci_ata_cmd() - PCI Bus Master for ATA R/W DMA commands
//
//***********************************************************

static int exec_pci_ata_cmd( unsigned char dev,
                             unsigned char * bufAddr,
                             long numSect );

static int exec_pci_ata_cmd( unsigned char dev,
                             unsigned char * bufAddr,
                             long numSect )

{
   unsigned char status;

   // Quit now if the command is incorrect.

   if (    ( reg_cmd_info.cmd != CMD_READ_DMA )
        && ( reg_cmd_info.cmd != CMD_READ_DMA_EXT )
        && ( reg_cmd_info.cmd != CMD_WRITE_DMA )
        && ( reg_cmd_info.cmd != CMD_WRITE_DMA_EXT ) )
   {
      reg_cmd_info.ec = 77;
      return 1;
   }

   // Set up the dma transfer

   if ( set_up_xfer( ( reg_cmd_info.cmd == CMD_WRITE_DMA )
                     ||
                     ( reg_cmd_info.cmd == CMD_WRITE_DMA_EXT ),
                     numSect * 512L, bufAddr ) )
   {
      reg_cmd_info.ec = 61;
      return 1;
   }

   // Set command time out.

   tmr_set_timeout();

   // Select the drive - call the sub_select function.
   // Quit now if this fails.

   if ( sub_select( dev ) )
   {
      return 1;
   }

   // Set up all the registers except the command register.

   sub_setup_command();

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.

   pio_outbyte( CB_CMD, reg_cmd_info.cmd );

   // The drive should start executing the command including any
   // data transfer.

   // Data transfer...
   // read the BMIDE regs
   // enable/start the dma channel.
   // read the BMIDE regs again

   pio_readBusMstrCmd();
   pio_readBusMstrStatus();
   pio_writeBusMstrCmd( (unsigned char) ( rwControl | BM_CR_MASK_START ) );
   pio_readBusMstrCmd();
   pio_readBusMstrStatus();

   // Data transfer...
   // the device and dma channel transfer the data here while we start
   // checking for command completion...
   // wait for the PCI BM Interrupt=1 (see ATAIOINT.C)...

   if ( SYSTEM_WAIT_INTR_OR_TIMEOUT() )       // time out ?
   {
      reg_cmd_info.to = 1;
      reg_cmd_info.ec = 73;
   }

   // End of command...
   // disable/stop the dma channel

   status = int_bmide_status;                // read BM status
   status &= ~ BM_SR_MASK_ACT;            // ignore Active bit
   pio_writeBusMstrCmd( BM_CR_MASK_STOP );    // shutdown DMA
   pio_readBusMstrCmd();                      // read BM cmd (just for trace)
   status |= pio_readBusMstrStatus();         // read BM status again

   if ( reg_cmd_info.ec == 0 )
   {
      if ( status & BM_SR_MASK_ERR )            // bus master error?
      {
         reg_cmd_info.ec = 78;                  // yes
      }
   }
   if ( reg_cmd_info.ec == 0 )
   {
      if ( status & BM_SR_MASK_ACT )            // end of PRD list?
      {
         reg_cmd_info.ec = 71;                  // no
      }
   }

   // End of command...
   // If no error use the Status register value that was read
   // by the interrupt handler. If there was an error
   // read the Status register because it may not have been
   // read by the interrupt handler.

   if ( reg_cmd_info.ec )
      status = pio_inbyte( CB_STAT );
   else
      status = int_ata_status;

   // Final status check...
   // if no error, check final status...
   // Error if BUSY, DEVICE FAULT, DRQ or ERROR status now.

   if ( reg_cmd_info.ec == 0 )
   {
      if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         reg_cmd_info.ec = 74;
      }
   }

   // Final status check...
   // if any error, update total bytes transferred.

   if ( reg_cmd_info.ec == 0 )
      reg_cmd_info.totalBytesXfer = numSect * 512L;
   else
      reg_cmd_info.totalBytesXfer = 0L;

   // All done.  The return values of this function are described in
   // MINDRVR.H.

   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

//***********************************************************
//
// dma_pci_lba28() - DMA in PCI Multiword for ATA R/W DMA
//
//***********************************************************

int dma_pci_lba28( unsigned char dev, unsigned char cmd,
                   unsigned int fr, unsigned int sc,
                   unsigned long lba,
                   unsigned char * bufAddr,
                   long numSect )

{

   // Setup current command information.

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = 0x00;      // nIEN=0 required on PCI !
   reg_cmd_info.ns  = numSect;
   reg_cmd_info.lbaSize = LBA28;
   reg_cmd_info.lbaHigh = 0L;
   reg_cmd_info.lbaLow = lba;

   // Execute the command.

   return exec_pci_ata_cmd( dev, bufAddr, numSect );
}

//***********************************************************
//
// dma_pci_lba48() - DMA in PCI Multiword for ATA R/W DMA
//
//***********************************************************

int dma_pci_lba48( unsigned char dev, unsigned char cmd,
                   unsigned int fr, unsigned int sc,
                   unsigned long lbahi, unsigned long lbalo,
                   unsigned char * bufAddr,
                   long numSect )

{

   // Setup current command information.

   reg_cmd_info.cmd = cmd;
   reg_cmd_info.fr = fr;
   reg_cmd_info.sc = sc;
   reg_cmd_info.dh = (unsigned char) ( CB_DH_LBA | ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   reg_cmd_info.dc = 0x00;      // nIEN=0 required on PCI !
   reg_cmd_info.ns  = numSect;
   reg_cmd_info.lbaSize = LBA48;
   reg_cmd_info.lbaHigh = lbahi;
   reg_cmd_info.lbaLow = lbalo;

   // Execute the command.

   return exec_pci_ata_cmd( dev, bufAddr, numSect );
}

#endif   // INCLUDE_ATA_DMA

#if INCLUDE_ATAPI_DMA

//***********************************************************
//
// dma_pci_packet() - PCI Bus Master for ATAPI Packet command
//
//***********************************************************

int dma_pci_packet( unsigned char dev,
                    unsigned int cpbc,
                    unsigned char * cdbBufAddr,
                    int dir,
                    long dpbc,
                    unsigned char * dataBufAddr )

{
   unsigned char status;

   // Make sure the command packet size is either 12 or 16
   // and save the command packet size and data.

   cpbc = cpbc < 12 ? 12 : cpbc;
   cpbc = cpbc > 12 ? 16 : cpbc;

   // Setup current command information.

   reg_cmd_info.cmd = CMD_PACKET;
   reg_cmd_info.fr = 0x01;      // packet DMA mode !
   reg_cmd_info.sc = 0;
   reg_cmd_info.sn = 0;
   reg_cmd_info.cl = 0;         // no Byte Count Limit in DMA !
   reg_cmd_info.ch = 0;         // no Byte Count Limit in DMA !
   reg_cmd_info.dh = (unsigned char) ( dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   reg_cmd_info.dc = 0x00;      // nIEN=0 required on PCI !
   reg_cmd_info.lbaSize = 0;

   // the data packet byte count must be even
   // and must not be zero

   if ( dpbc & 1L )
      dpbc ++ ;
   if ( dpbc < 2L )
      dpbc = 2L;

   // Set up the dma transfer

   if ( set_up_xfer( dir, dpbc, dataBufAddr ) )
   {
      reg_cmd_info.ec = 61;
      return 1;
   }

   // Set command time out.

   tmr_set_timeout();

   // Select the drive - call the reg_select function.
   // Quit now if this fails.

   if ( sub_select( dev ) )
   {
      return 1;
   }

   // Set up all the registers except the command register.

   sub_setup_command();

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.

   pio_outbyte( CB_CMD, CMD_PACKET );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not started the
   // command yet.

   DELAY400NS;

   // Command packet transfer...
   // Poll Alternate Status for BSY=0.

   while ( 1 )
   {
      status = pio_inbyte( CB_ASTAT );       // poll for not busy
      if ( ( status & CB_STAT_BSY ) == 0 )
         break;
      if ( tmr_chk_timeout() )               // time out yet ?
      {
         reg_cmd_info.to = 1;
         reg_cmd_info.ec = 75;
         break;
      }
   }

   // Command packet transfer...
   // Check for protocol failures... no interrupt here please!

   // Command packet transfer...
   // If no error, transfer the command packet.

   if ( reg_cmd_info.ec == 0 )
   {

      // Command packet transfer...
      // Read the primary status register and the other ATAPI registers.

      status = pio_inbyte( CB_STAT );

      // Command packet transfer...
      // check status: must have BSY=0, DRQ=1 now

      if (    ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) )
           != CB_STAT_DRQ
         )
      {
         reg_cmd_info.ec = 76;
      }
      else
      {

         // Command packet transfer...
         // xfer the command packet (the cdb)

         pio_drq_block_out( CB_DATA, cdbBufAddr, cpbc >> 1 );
      }
   }

   // Data transfer...
   // The drive should start executing the command
   // including any data transfer.
   // If no error, set up and start the DMA,
   // and wait for the DMA to complete.

   if ( reg_cmd_info.ec == 0 )
   {

      // Data transfer...
      // read the BMIDE regs
      // enable/start the dma channel.
      // read the BMIDE regs again

      pio_readBusMstrCmd();
      pio_readBusMstrStatus();
      pio_writeBusMstrCmd( (unsigned char) ( rwControl | BM_CR_MASK_START ) );
      pio_readBusMstrCmd();
      pio_readBusMstrStatus();

      // Data transfer...
      // the device and dma channel transfer the data here while we start
      // checking for command completion...
      // wait for the PCI BM Active=0 and Interrupt=1 or PCI BM Error=1...

      if ( SYSTEM_WAIT_INTR_OR_TIMEOUT() )    // time out ?
      {
         reg_cmd_info.to = 1;
         reg_cmd_info.ec = 73;
      }

      // End of command...
      // disable/stop the dma channel

      status = int_bmide_status;                // read BM status
      status &= ~ BM_SR_MASK_ACT;            // ignore Active bit
      pio_writeBusMstrCmd( BM_CR_MASK_STOP );    // shutdown DMA
      pio_readBusMstrCmd();                      // read BM cmd (just for trace)
      status |= pio_readBusMstrStatus();         // read BM statu again
   }

   if ( reg_cmd_info.ec == 0 )
   {
      if ( status & ( BM_SR_MASK_ERR ) )        // bus master error?
      {
         reg_cmd_info.ec = 78;                  // yes
      }
      if ( ( status & BM_SR_MASK_ACT ) )        // end of PRD list?
      {
         reg_cmd_info.ec = 71;                  // no
      }
   }

   // End of command...
   // If no error use the Status register value that was read
   // by the interrupt handler. If there was an error
   // read the Status register because it may not have been
   // read by the interrupt handler.

   if ( reg_cmd_info.ec )
      status = pio_inbyte( CB_STAT );
   else
      status = int_ata_status;

   // Final status check...
   // if no error, check final status...
   // Error if BUSY, DRQ or ERROR status now.

   if ( reg_cmd_info.ec == 0 )
   {
      if ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         reg_cmd_info.ec = 74;
      }
   }


   // Final status check...
   // if any error, update total bytes transferred.

   if ( reg_cmd_info.ec == 0 )
      reg_cmd_info.totalBytesXfer = dpbc;
   else
      reg_cmd_info.totalBytesXfer = 0L;

   // All done.  The return values of this function are described in
   // MINDRVR.H.

   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

#endif   // INCLUDE_ATAPI_DMA

//*************************************************************
//
// sub_setup_command() -- setup the command parameters
//                        in FR, SC, SN, CL, CH and DH.
//
//*************************************************************

static void sub_setup_command( void )

{

   // output DevCtrl - same for all devices and commands
   pio_outbyte( CB_DC, reg_cmd_info.dc );

   // output command parameters
   if ( reg_cmd_info.lbaSize == LBA28 )
   {
      // in ATA LBA28 mode
      pio_outbyte( CB_FR, (unsigned char) reg_cmd_info.fr );
      pio_outbyte( CB_SC, (unsigned char) reg_cmd_info.sc );
      pio_outbyte( CB_SN, (unsigned char) reg_cmd_info.lbaLow );
      pio_outbyte( CB_CL, (unsigned char) ( reg_cmd_info.lbaLow >> 8 ) );
      pio_outbyte( CB_CH, (unsigned char) ( reg_cmd_info.lbaLow >> 16 ) );
      pio_outbyte( CB_DH, (unsigned char) ( ( reg_cmd_info.dh & 0xf0 )
                                            | ( ( reg_cmd_info.lbaLow >> 24 ) & 0x0f ) ) );
   }
   else
   if ( reg_cmd_info.lbaSize == LBA48 )
   {
      // in ATA LBA48 mode
      pio_outbyte( CB_FR, (unsigned char) ( reg_cmd_info.fr >> 8 ) );
      pio_outbyte( CB_SC, (unsigned char) ( reg_cmd_info.sc >> 8 ) );
      pio_outbyte( CB_SN, (unsigned char) ( reg_cmd_info.lbaLow >> 24 ) );
      pio_outbyte( CB_CL, (unsigned char) reg_cmd_info.lbaHigh );
      pio_outbyte( CB_CH, (unsigned char) ( reg_cmd_info.lbaHigh >> 8 ) );
      pio_outbyte( CB_FR, (unsigned char) reg_cmd_info.fr );
      pio_outbyte( CB_SC, (unsigned char) reg_cmd_info.sc );
      pio_outbyte( CB_SN, (unsigned char) reg_cmd_info.lbaLow );
      pio_outbyte( CB_CL, (unsigned char) ( reg_cmd_info.lbaLow >> 8 ) );
      pio_outbyte( CB_CH, (unsigned char) ( reg_cmd_info.lbaLow >> 16 ) );
      pio_outbyte( CB_DH, reg_cmd_info.dh  );
   }
   else
   {
      // for ATAPI PACKET command
      pio_outbyte( CB_FR, (unsigned char) reg_cmd_info.fr  );
      pio_outbyte( CB_SC, (unsigned char) reg_cmd_info.sc  );
      pio_outbyte( CB_SN, (unsigned char) reg_cmd_info.sn  );
      pio_outbyte( CB_CL, (unsigned char) reg_cmd_info.cl  );
      pio_outbyte( CB_CH, (unsigned char) reg_cmd_info.ch  );
      pio_outbyte( CB_DH, reg_cmd_info.dh  );
   }
}

//*************************************************************
//
// sub_trace_command() -- trace the end of a command.
//
//*************************************************************

static void sub_trace_command( void )

{

   reg_cmd_info.st = pio_inbyte( CB_STAT );
   reg_cmd_info.as = pio_inbyte( CB_ASTAT );
   reg_cmd_info.er = pio_inbyte( CB_ERR );

// !!! if you want to read back the other device registers
// !!! at the end of a command then this is the place to do
// !!! it. The code here is just and example of out this is
// !!! done on a little endian system like an x86.

#if 0    // read back other registers

   {
      unsigned long lbaHigh;
      unsigned long lbaLow;
      unsigned char sc48[2];
      unsigned char lba48[8];

      lbaHigh = 0;
      lbaLow = 0;
      if ( reg_cmd_info.lbaSize == LBA48 )
      {
         // read back ATA LBA48...
         sc48[0]  = pio_inbyte( CB_SC );
         lba48[0] = pio_inbyte( CB_SN );
         lba48[1] = pio_inbyte( CB_CL );
         lba48[2] = pio_inbyte( CB_CH );
         pio_outbyte( CB_DC, CB_DC_HOB );
         sc48[1]  = pio_inbyte( CB_SC );
         lba48[3] = pio_inbyte( CB_SN );
         lba48[4] = pio_inbyte( CB_CL );
         lba48[5] = pio_inbyte( CB_CH );
         lba48[6] = 0;
         lba48[7] = 0;
         lbaHigh = * (unsigned long *) ( lba48 + 4 );
         lbaLow  = * (unsigned long *) ( lba48 + 0 );
      }
      else
      if ( reg_cmd_info.lbaSize == LBA28 )
      {
         // read back ATA LBA28
         lbaLow = pio_inbyte( CB_DH );
         lbaLow = lbaLow << 8;
         lbaLow = lbaLow | pio_inbyte( CB_CH );
         lbaLow = lbaLow << 8;
         lbaLow = lbaLow | pio_inbyte( CB_CL );
         lbaLow = lbaLow << 8;
         lbaLow = lbaLow | pio_inbyte( CB_SN );
      }
      else
      {
         // really no reason to read back for ATAPI
      }
   }

#endif   // read back other registers

}

//*************************************************************
//
// sub_select() - function used to select a drive.
//
// Function to select a drive making sure that BSY=0 and DRQ=0.
//
//**************************************************************

static int sub_select( unsigned char dev )

{
   unsigned char status;

   // PAY ATTENTION HERE
   // The caller may want to issue a command to a device that doesn't
   // exist (for example, Exec Dev Diag), so if we see this,
   // just select that device, skip all status checking and return.
   // We assume the caller knows what they are doing!

   if ( reg_config_info[dev] < REG_CONFIG_TYPE_ATA )
   {
      // select the device and return

      pio_outbyte( CB_DH, (unsigned char) ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
      DELAY400NS;
      return 0;
   }

   // The rest of this is the normal ATA stuff for device selection
   // and we don't expect the caller to be selecting a device that
   // does not exist.
   // We don't know which drive is currently selected but we should
   // wait BSY=0 and DRQ=0. Normally both BSY=0 and DRQ=0
   // unless something is very wrong!

   while ( 1 )
   {
      status = pio_inbyte( CB_STAT );
      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == 0 )
         break;
      if ( tmr_chk_timeout() )
      {
         reg_cmd_info.to = 1;
         reg_cmd_info.ec = 11;
         reg_cmd_info.st = status;
         reg_cmd_info.as = pio_inbyte( CB_ASTAT );
         reg_cmd_info.er = pio_inbyte( CB_ERR );
         return 1;
      }
   }

   // Here we select the drive we really want to work with by
   // setting the DEV bit in the Drive/Head register.

   pio_outbyte( CB_DH, (unsigned char) ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) );
   DELAY400NS;

   // Wait for the selected device to have BSY=0 and DRQ=0.
   // Normally the drive should be in this state unless
   // something is very wrong (or initial power up is still in
   // progress).

   while ( 1 )
   {
      status = pio_inbyte( CB_STAT );
      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == 0 )
         break;
      if ( tmr_chk_timeout() )
      {
         reg_cmd_info.to = 1;
         reg_cmd_info.ec = 12;
         reg_cmd_info.st = status;
         reg_cmd_info.as = pio_inbyte( CB_ASTAT );
         reg_cmd_info.er = pio_inbyte( CB_ERR );
         return 1;
      }
   }

   // All done.  The return values of this function are described in
   // ATAIO.H.

   if ( reg_cmd_info.ec )
      return 1;
   return 0;
}

//*************************************************************
//
// sub_wait_poll() - wait for interrupt or poll for BSY=0
//
//*************************************************************

static void sub_wait_poll( unsigned char we, unsigned char pe )

{
   unsigned char status;

   // Wait for interrupt -or- wait for not BUSY -or- wait for time out.

   if ( we && int_use_intr_flag )
   {
      if ( SYSTEM_WAIT_INTR_OR_TIMEOUT() )    // time out ?
      {
         reg_cmd_info.to = 1;
         reg_cmd_info.ec = we;
      }
   }
   else
   {
      while ( 1 )
      {
         status = pio_inbyte( CB_ASTAT );       // poll for not busy
         if ( ( status & CB_STAT_BSY ) == 0 )
            break;
         if ( tmr_chk_timeout() )               // time out yet ?
         {
            reg_cmd_info.to = 1;
            reg_cmd_info.ec = pe;
            break;
         }
      }
   }
}

//***********************************************************
//
// functions used to read/write the BMIDE registers
//
//***********************************************************
/*
static unsigned char pio_readBusMstrCmd( void )

{
   unsigned char x;

   if ( ! pio_bmide_base_addr )
      return 0;
   x = * (pio_bmide_base_addr + BM_COMMAND_REG );
   return x;
}


static unsigned char pio_readBusMstrStatus( void )

{
   unsigned char x;

   if ( ! pio_bmide_base_addr )
      return 0;
   x = * ( pio_bmide_base_addr + BM_STATUS_REG );
   return x;
}


static void pio_writeBusMstrCmd( unsigned char x )

{

   if ( ! pio_bmide_base_addr )
      return;
   * ( pio_bmide_base_addr + BM_COMMAND_REG ) = x;
}


static void pio_writeBusMstrStatus( unsigned char x )

{

   if ( ! pio_bmide_base_addr )
      return;
   * ( pio_bmide_base_addr + BM_STATUS_REG ) =  x;
}
*/
//*************************************************************
//
// These functions do basic IN/OUT of byte and word values:
//
//    pio_inbyte()
//    pio_outbyte()
//    pio_inword()
//    pio_outword()
//
//*************************************************************

//static unsigned char pio_inbyte( unsigned char addr )
//
//{
//
//   //!!! read an 8-bit ATA register
//
//   return * pio_reg_addrs[ addr ];
//}

//*************************************************************

//static void pio_outbyte( int addr, unsigned char data )
//
//{
//
//   //!!! write an 8-bit ATA register
//
//   * pio_reg_addrs[ addr ] = data;
//}

//*************************************************************

//static unsigned int pio_inword( unsigned char addr )
//
//{
//
//   //!!! read an 8-bit ATA register (usually the ATA Data register)
//
//   return * ( (unsigned int *) pio_reg_addrs[ addr ] );
//}

//*************************************************************

//static void pio_outword( int addr, unsigned int data )
//
//{
//
//   //!!! Write an 8-bit ATA register (usually the ATA Data register)
//
//   * ( (unsigned int *) pio_reg_addrs[ addr ] ) = data;
//}

//*************************************************************

//static unsigned long pio_indword( unsigned char addr )
//
//{
//
//   //!!! read an 8-bit ATA register (usually the ATA Data register)
//
//   return * ( (unsigned long *) pio_reg_addrs[ addr ] );
//}

//*************************************************************

//static void pio_outdword( int addr, unsigned long data )
//
//{
//
//   //!!! Write an 8-bit ATA register (usually the ATA Data register)
//
//   * ( (unsigned long *) pio_reg_addrs[ addr ] ) = data;
//}

//*************************************************************
//
// These functions are normally used to transfer DRQ blocks:
//
// pio_drq_block_in()
// pio_drq_block_out()
//
//*************************************************************

// Note: pio_drq_block_in() is the primary way perform PIO
// Data In transfers. It will handle 8-bit, 16-bit and 32-bit
// I/O based data transfers.

static void pio_drq_block_in( unsigned char addrDataReg,
                       unsigned char * bufAddr,
                       long wordCnt )

{

   // NOTE: wordCnt is the size of a DRQ data block/packet
   // in words. The maximum value of wordCnt is normally:
   // a) For ATA, 16384 words or 32768 bytes (64 sectors,
   //    only with READ/WRITE MULTIPLE commands),
   // b) For ATAPI, 32768 words or 65536 bytes
   //    (actually 65535 bytes plus a pad byte).

   {
      int pxw;
      long wc;

      // adjust pio_xfer_width - don't use DWORD if wordCnt is odd.

      pxw = pio_xfer_width;
      if ( ( pxw == 32 ) && ( wordCnt & 0x00000001L ) )
         pxw = 16;

      // Data transfer using INS instruction.
      // Break the transfer into chunks of 32768 or fewer bytes.

      while ( wordCnt > 0 )
      {
         if ( wordCnt > 16384L )
            wc = 16384;
         else
            wc = wordCnt;
         if ( pxw == 8 )
         {
            // do REP INS
            pio_rep_inbyte( addrDataReg, bufAddr, wc * 2L );
         }
         else
         if ( pxw == 32 )
         {
            // do REP INSD
            pio_rep_indword( addrDataReg, bufAddr, wc / 2L );
         }
         else
         {
            // do REP INSW
            pio_rep_inword( addrDataReg, bufAddr, wc );
         }
         bufAddr = bufAddr + ( wc * 2 );
         wordCnt = wordCnt - wc;
      }
   }

   return;
}

//*************************************************************

// Note: pio_drq_block_out() is the primary way perform PIO
// Data Out transfers. It will handle 8-bit, 16-bit and 32-bit
// I/O based data transfers.

static void pio_drq_block_out( unsigned char addrDataReg,
                        unsigned char * bufAddr,
                        long wordCnt )

{

   // NOTE: wordCnt is the size of a DRQ data block/packet
   // in words. The maximum value of wordCnt is normally:
   // a) For ATA, 16384 words or 32768 bytes (64 sectors,
   //    only with READ/WRITE MULTIPLE commands),
   // b) For ATAPI, 32768 words or 65536 bytes
   //    (actually 65535 bytes plus a pad byte).

   {
      int pxw;
      long wc;

      // adjust pio_xfer_width - don't use DWORD if wordCnt is odd.

      pxw = pio_xfer_width;
      if ( ( pxw == 32 ) && ( wordCnt & 0x00000001L ) )
         pxw = 16;

      // Data transfer using OUTS instruction.
      // Break the transfer into chunks of 32768 or fewer bytes.

      while ( wordCnt > 0 )
      {
         if ( wordCnt > 16384L )
            wc = 16384;
         else
            wc = wordCnt;
         if ( pxw == 8 )
         {
            // do REP OUTS
            pio_rep_outbyte( addrDataReg, bufAddr, wc * 2L );
         }
         else
         if ( pxw == 32 )
         {
            // do REP OUTSD
            pio_rep_outdword( addrDataReg, bufAddr, wc / 2L );
         }
         else
         {
            // do REP OUTSW
            pio_rep_outword( addrDataReg, bufAddr, wc );
         }
         bufAddr = bufAddr + ( wc * 2 );
         wordCnt = wordCnt - wc;
      }
   }

   return;
}

//*************************************************************
//
// These functions transfer PIO DRQ data blocks through the ATA
// Data register. On an x86 these functions would use the
// REP INS and REP OUTS instructions.
//
// pio_rep_inbyte()
// pio_rep_outbyte()
// pio_rep_inword()
// pio_rep_outword()
// pio_rep_indword()
// pio_rep_outdword()
//
// These functions can be called directly but usually they
// are called by the pio_drq_block_in() and pio_drq_block_out()
// functions to perform I/O mode transfers. See the
// pio_xfer_width variable!
//
//*************************************************************

static void pio_rep_inbyte( unsigned char addrDataReg,
                     unsigned char * bufAddr,
                     long byteCnt )

{

   // Warning: Avoid calling this function with
   // byteCnt > 32768 (transfers 32768 bytes).
   // that bufOff is a value between 0 and 15 (0xf).

   //!!! repeat read an 8-bit register (ATA Data register when
   //!!! ATA status is BSY=0 DRQ=1). For example:

   while ( byteCnt > 0 )
   {
      * bufAddr = pio_inbyte( addrDataReg );
      bufAddr ++ ;
      byteCnt -- ;
   }
}

//*************************************************************

static void pio_rep_outbyte( unsigned char addrDataReg,
                      unsigned char * bufAddr,
                      long byteCnt )

{

   // Warning: Avoid calling this function with
   // byteCnt > 32768 (transfers 32768 bytes).
   // that bufOff is a value between 0 and 15 (0xf).

   //!!! repeat write an 8-bit register (ATA Data register when
   //!!! ATA status is BSY=0 DRQ=1). For example:

   while ( byteCnt > 0 )
   {
      pio_outbyte( addrDataReg, * bufAddr );
      bufAddr ++ ;
      byteCnt -- ;
   }
}

//*************************************************************

static void pio_rep_inword( unsigned char addrDataReg,
                     unsigned char * bufAddr,
                     long wordCnt )

{

   // Warning: Avoid calling this function with
   // wordCnt > 16384 (transfers 32768 bytes).

   //!!! repeat read a 16-bit register (ATA Data register when
   //!!! ATA status is BSY=0 DRQ=1). For example:

   while ( wordCnt > 0 )
   {
      * (unsigned int *) bufAddr = pio_inword( addrDataReg );
      bufAddr += 2;
      wordCnt -- ;
   }
}

//*************************************************************

static void pio_rep_outword( unsigned char addrDataReg,
                      unsigned char * bufAddr,
                      long wordCnt )

{

   // Warning: Avoid calling this function with
   // wordCnt > 16384 (transfers 32768 bytes).

   //!!! repeat write a 16-bit register (ATA Data register when
   //!!! ATA status is BSY=0 DRQ=1). For example:

   while ( wordCnt > 0 )
   {
      pio_outword( addrDataReg, * (unsigned int *) bufAddr );
      bufAddr += 2;
      wordCnt -- ;
   }
}

//*************************************************************

static void pio_rep_indword( unsigned char addrDataReg,
                      unsigned char * bufAddr,
                      long dwordCnt )

{

   // Warning: Avoid calling this function with
   // dwordCnt > 8192 (transfers 32768 bytes).

   //!!! repeat read a 32-bit register (ATA Data register when
   //!!! ATA status is BSY=0 DRQ=1). For example:

   while ( dwordCnt > 0 )
   {
      * (unsigned long *) bufAddr = pio_indword( addrDataReg );
      bufAddr += 4;
      dwordCnt -- ;
   }
}

//*************************************************************

static void pio_rep_outdword( unsigned char addrDataReg,
                       unsigned char * bufAddr,
                       long dwordCnt )

{

   // Warning: Avoid calling this function with
   // dwordCnt > 8192 (transfers 32768 bytes).

   //!!! repeat write a 32-bit register (ATA Data register when
   //!!! ATA status is BSY=0 DRQ=1). For example:

   while ( dwordCnt > 0 )
   {
      pio_outdword( addrDataReg, * (unsigned long *) bufAddr );
      bufAddr += 4;
      dwordCnt -- ;
   }
}


//*************************************************************
//
// Command timing functions
//
//**************************************************************


static long tmr_cmd_start_time;      // command start time - see the
                              // tmr_set_timeout() and
                              // tmr_chk_timeout() functions.

//*************************************************************
//
// tmr_set_timeout() - get the command start time
//
//**************************************************************

static void tmr_set_timeout( void )

{

   // get the command start time
   tmr_cmd_start_time = SYSTEM_READ_TIMER();
}

//*************************************************************
//
// tmr_chk_timeout() - check for command timeout.
//
// Gives non-zero return if command has timed out.
//
//**************************************************************

static int tmr_chk_timeout( void )

{
   long curTime;

   // get current time
   curTime = SYSTEM_READ_TIMER();

   // timed out yet ?
   if ( curTime >= ( tmr_cmd_start_time
                     + ( TMR_TIME_OUT * SYSTEM_TIMER_TICKS_PER_SECOND ) ) )
      return 1;      // yes

   // no timeout yet
   return 0;
}

// end mindrvr.c
