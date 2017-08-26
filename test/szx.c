/* szx.c: SZX test routines
   Copyright (c) 2017 Philip Kendall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

/* This file contains a number of routines for checking the contents of an SZX
   file. It very deliberately does not use any of the core libspectrum code for
   this as that would defeat the point of a unit test */

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internals.h"
#include "test.h"

typedef struct szx_chunk_t {
  libspectrum_byte *data;
  size_t length;
} szx_chunk_t;

static szx_chunk_t*
find_szx_chunk( libspectrum_buffer *buffer, const char *search )
{
  libspectrum_byte *data;
  size_t data_remaining;
  libspectrum_byte id[5] = {0};
  libspectrum_byte length_buffer[4];
  libspectrum_dword length;

  data = libspectrum_buffer_get_data( buffer );
  data_remaining = libspectrum_buffer_get_data_size( buffer );

  if( data_remaining < 0 ) {
    fprintf( stderr, "SZX file is less than 8 bytes long\n" );
    return NULL;
  }

  /* Skip header */
  data += 8; data_remaining -= 8;

  while( data_remaining > 0 ) {
    if( data_remaining < 8 ) {
      fprintf( stderr, "Chunk is less than 8 bytes long\n" );
      return NULL;
    }

    memcpy( id, data, 4 );
    data += 4; data_remaining -= 4;

    memcpy( length_buffer, data, 4 );
    data += 4; data_remaining -= 4;

    length = length_buffer[0] + (length_buffer[1] << 8) +
      (length_buffer[2] << 16) + (length_buffer[3] << 24);

    if( data_remaining < length ) {
      fprintf( stderr, "Not enough data for chunk\n" );
      return NULL;
    }

    if( !memcmp( id, search, 4 ) ) {
      szx_chunk_t *chunk = libspectrum_malloc( sizeof(*chunk) );
      libspectrum_byte *chunk_data = libspectrum_malloc( length );
      memcpy( chunk_data, data, length );
      chunk->data = chunk_data;
      chunk->length = length;
      return chunk;
    }

    data += length; data_remaining -= length;
  }

  return NULL;
}

static test_return_t
szx_block_test( const char *id, libspectrum_machine machine,
    void (*setter)( libspectrum_snap* ),
    libspectrum_byte *expected, size_t expected_length )
{
  libspectrum_snap *snap;
  libspectrum_buffer *buffer;
  int out_flags;
  szx_chunk_t *chunk;
  test_return_t r = TEST_INCOMPLETE;

  buffer = libspectrum_buffer_alloc();

  snap = libspectrum_snap_alloc();

  libspectrum_snap_set_machine( snap, machine );

  setter( snap );

  libspectrum_szx_write( buffer, &out_flags, snap, NULL, 0 );
  libspectrum_snap_free( snap );

  chunk = find_szx_chunk( buffer, id );
  if( !chunk ) {
    fprintf( stderr, "Chunk not found\n" );
    return TEST_FAIL;
  }

  libspectrum_buffer_free( buffer );

  if( chunk->length == expected_length ) {
    if( memcmp( chunk->data, expected, expected_length ) ) {
      fprintf( stderr, "Chunk has wrong data\n" );
      r = TEST_FAIL;
    } else {
      r = TEST_PASS;
    }
  } else {
    fprintf( stderr, "Chunk has wrong length\n" );
    r = TEST_FAIL;
  }

  libspectrum_free( chunk->data );
  libspectrum_free( chunk );

  return r;
}

static void
z80r_setter( libspectrum_snap *snap )
{
  libspectrum_snap_set_a( snap, 0xc4 );
  libspectrum_snap_set_f( snap, 0x1f );
  libspectrum_snap_set_bc( snap, 0x0306 );
  libspectrum_snap_set_de( snap, 0x06e4 );
  libspectrum_snap_set_hl( snap, 0x0154 );

  libspectrum_snap_set_a_( snap, 0x69 );
  libspectrum_snap_set_f_( snap, 0x07 );
  libspectrum_snap_set_bc_( snap, 0xe7dc );
  libspectrum_snap_set_de_( snap, 0xc3d0 );
  libspectrum_snap_set_hl_( snap, 0xdccb );

  libspectrum_snap_set_ix( snap, 0x8ba3 );
  libspectrum_snap_set_iy( snap, 0x1c13 );
  libspectrum_snap_set_sp( snap, 0xf86d );
  libspectrum_snap_set_pc( snap, 0xc81e );

  libspectrum_snap_set_i( snap, 0x19 );
  libspectrum_snap_set_r( snap, 0x84 );
  libspectrum_snap_set_iff1( snap, 1 );
  libspectrum_snap_set_iff2( snap, 0 );
  libspectrum_snap_set_im( snap, 2 );

  libspectrum_snap_set_tstates( snap, 40 );

  libspectrum_snap_set_last_instruction_ei( snap, 1 );
  libspectrum_snap_set_halted( snap, 0 );
  libspectrum_snap_set_last_instruction_set_f( snap, 1 );

  libspectrum_snap_set_memptr( snap, 0xdc03 );
}

static libspectrum_byte
test_31_expected[] = {
  0x1f, 0xc4, 0x06, 0x03, 0xe4, 0x06, 0x54, 0x01, /* AF, BC, DE, HL */
  0x07, 0x69, 0xdc, 0xe7, 0xd0, 0xc3, 0xcb, 0xdc, /* AF', BC', DE', HL' */
  0xa3, 0x8b, 0x13, 0x1c, 0x6d, 0xf8, 0x1e, 0xc8, /* IX, IY, SP, PC */
  0x19, 0x84, 0x01, 0x00, 0x02, /* I, R, IFF1, IFF2, IM */
  0x28, 0x00, 0x00, 0x00, 0x08, /* tstates, tstates until /INT goes high */
  0x05, /* flags */
  0x03, 0xdc /* MEMPTR */
};

test_return_t
test_31( void )
{
  return szx_block_test( "Z80R", LIBSPECTRUM_MACHINE_48, z80r_setter,
      test_31_expected, ARRAY_SIZE(test_31_expected) );
}

static void
spcr_setter( libspectrum_snap *snap )
{
  libspectrum_snap_set_out_ula( snap, 0xfa );
  libspectrum_snap_set_out_128_memoryport( snap, 0x6f );
  libspectrum_snap_set_out_plus3_memoryport( snap, 0x28 );
}

static libspectrum_byte
test_32_expected[] = {
  0x02, 0x6f, 0x28, 0xfa, /* Border, 128, +3, ULA */
  0x00, 0x00, 0x00, 0x00 /* Reserved */
};

test_return_t
test_32( void )
{
  return szx_block_test( "SPCR", LIBSPECTRUM_MACHINE_PLUS3, spcr_setter,
      test_32_expected, ARRAY_SIZE(test_32_expected) );
}

static void
joy_setter( libspectrum_snap *snap )
{
  libspectrum_snap_set_joystick_active_count( snap, 2 );
  libspectrum_snap_set_joystick_list( snap, 0, LIBSPECTRUM_JOYSTICK_KEMPSTON );
  libspectrum_snap_set_joystick_inputs( snap, 0,
      LIBSPECTRUM_JOYSTICK_INPUT_JOYSTICK_1 );
  libspectrum_snap_set_joystick_list( snap, 1, LIBSPECTRUM_JOYSTICK_SINCLAIR_1 );
  libspectrum_snap_set_joystick_inputs( snap, 1,
      LIBSPECTRUM_JOYSTICK_INPUT_JOYSTICK_2 );
}

static libspectrum_byte
test_33_expected[] = {
  0x01, 0x00, 0x00, 0x00, /* Flags */
  0x00, 0x03 /* Joystick 1 = Kempston, Joystick 2 = Sinclair 1 */
};

test_return_t
test_33( void )
{
  return szx_block_test( "JOY\0", LIBSPECTRUM_MACHINE_48, joy_setter,
      test_33_expected, ARRAY_SIZE(test_33_expected) );
}

static void
keyb_setter( libspectrum_snap *snap )
{
  libspectrum_snap_set_issue2( snap, 1 );
  libspectrum_snap_set_joystick_active_count( snap, 1 );
  libspectrum_snap_set_joystick_list( snap, 0, LIBSPECTRUM_JOYSTICK_CURSOR );
  libspectrum_snap_set_joystick_inputs( snap, 0,
      LIBSPECTRUM_JOYSTICK_INPUT_KEYBOARD );
}

static libspectrum_byte
test_34_expected[] = {
  0x01, 0x00, 0x00, 0x00, /* Flags */
  0x02 /* Cursor joystick */
};

test_return_t
test_34( void )
{
  return szx_block_test( "KEYB", LIBSPECTRUM_MACHINE_48, keyb_setter,
      test_34_expected, ARRAY_SIZE(test_34_expected) );
}

static void
zxpr_setter( libspectrum_snap *snap )
{
  libspectrum_snap_set_zx_printer_active( snap, 1 );
}

static libspectrum_byte
test_35_expected[] = {
  0x01, 0x00 /* Flags */
};

test_return_t
test_35( void )
{
  return szx_block_test( "ZXPR", LIBSPECTRUM_MACHINE_48, zxpr_setter,
      test_35_expected, ARRAY_SIZE(test_35_expected) );
}
