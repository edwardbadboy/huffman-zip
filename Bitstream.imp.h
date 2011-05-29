// Copyright (c) 2006-2008 John P. Costella.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software")
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included 
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT, OR OTHERWISE, ARISING
// FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// Costella/Bitstream.imp.h: 
//   Costella::Bitstream implementation.
//
//   Class begun: February 17, 2006.
//   This version: May 5, 2008.
//
//   Written in ISO C++98.


// Include files.
#include "Bitstream.h"
#include <sstream>


// Put inside the Costella::Bitstream namespace.
namespace Costella{ namespace Bitstream{


// Implementation prototypes.
template< typename ValueType >
ValueType mask( int n );
template< typename ValueType >
std::vector< ValueType > leadingBitVector();


// Static members.
template< typename PositionType >
Out< PositionType > Out< PositionType >::null;


// Out::Out():
//   Constructor. 
template< typename PositionType >
Out< PositionType >::Out( std::ostream& outstream ) :
  outstream_( outstream ),
  position_( 0 ),
  numBitsWaiting_( 0 ),
  waitingBits_( 0 )
{
}


// Out::~Out():
//   Destructor.
template< typename PositionType >
Out< PositionType >::~Out()
{
  // Flush out any waiting bits.
  flush();
}


// Out::bits():
//   Write a number of bits from a vector of bytes.
template< typename PositionType >
template< typename NumBitsType >
void Out< PositionType >::bits( const std::vector< unsigned char >& data,
    NumBitsType numBits )
{
  // Set up invalid mask during instantiation.
  static const NumBitsType invalidMask = mask< NumBitsType >( -1 );

  // Check for a valid number of bits.
  static const std::string f = "Costella::Bitstream::Out::bits(): ";
  if( numBits & invalidMask )
  {
    std::ostringstream oss;
    oss << f << "Invalid negative number of bits (" << numBits << ")";
    throw oss.str();
  }
  else if( numBits > static_cast< NumBitsType >( data.size() ) << 3 )
  {
    std::ostringstream oss;
    oss << f << "Number of bits (" << numBits << 
      ") exceeds capabilities of vector of size " << data.size() << " (" <<
      ( data.size() << 3 ) << ")";
    throw oss.str();
  }

  // Walk along the vector of bytes, and run down the number of bits, until
  // there are no bits left to write. Loop until there are eight or fewer
  // bits left to write.
  std::vector< unsigned char >::const_iterator it = data.begin();
  for( ; numBits > 8; it++, numBits -= 8 )
  {
    // Write the bits of this byte.
    bitsByte( *it );
  }

  // Now do the final eight or fewer bits.
  bitsByte( *it, static_cast< int >( numBits ) );
}


// Out::fixed():
//   Write a non-negative integer with a fixed bit-width.
template< typename PositionType >
template< typename ValueType >
void Out< PositionType >::fixed( ValueType value, int width )
{
  // Set up invalid mask during instantiation.
  static const ValueType invalidMask = mask< ValueType >( -1 );

  // Check that the value is non-negative.
  static const std::string f = "Costella::Bitstream::Out::fixed(): ";
  if( value & invalidMask )
  {
    std::ostringstream oss;
    oss << f << "Negative value (" << value << ") not allowed";
    throw oss.str(); 
  }

  // Check that the bit-width is valid.
  if( width < 0 )
  {
    std::ostringstream oss;
    oss << f << "Negative bit-width (" << width << ") not allowed"; 
    throw oss.str(); 
  }
  else if( width > std::numeric_limits< ValueType >::digits )
  {
    std::ostringstream oss;
    oss << f << "Specified bit-width (" << width << 
      ") too large for value's type (maximum width " << std::numeric_limits<
      ValueType >::digits << ")";
    throw oss.str(); 
  }

  // Keep going until we have no more bits to do.
  for( ; width > 0; width -= 8 )
  {
    // Check if we have at least eight bits left to do.
    unsigned char byte;
    if( width >= 8 )
    {
      // We have at least 8 bits left to do. Move them into position.
      byte = static_cast< unsigned char >( value >> ( width - 8 ) );
    }
    else
    {
      // We have fewer than 8 bits left to do. Move them into position.
      byte = static_cast< unsigned char >( value << ( 8 - width ) );
    }

    // Write them.
    bitsByte( byte, ( width >= 8 ) ? 8 : width );
  }
}


// Out::variable():
//   Write a positive integer with a variable bit-width. The argument
//   ldMaxWidth (log to base 2 of the maximum bit-width) should be 
//     ceil( ld( maxWidth ) )
//   i.e., 
//     0 if the value must be 0x1 (1 bit) (useless but possible)
//     1 if the value is in [ 0x1, 0x3 ] (2 bits)
//     2 for [ 0x1, 0xf ] (4 bits)
//     3 for [ 0x1, 0xff ] (8 bits)
//     4 for [ 0x1, 0xffff ] (16 bits)
//     5 for [ 0x1, 0xffff:ffff ] (32 bits)
//     6 for [ 0x1, 0xffff:ffff:ffff:ffff ] (64 bits)
//     7 for [ 0x1, 0xffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ] (128 bits) 
//   The function returns the total number of bits required.
template< typename PositionType >
template< typename ValueType >
int Out< PositionType >::variable( ValueType value, int ldMaxWidth )  
{
  // Check that value is not zero.
  static const std::string f = "Costella::Bitstream::Out::variable(): ";
  if( !value )
  {
    throw f + "Value must be positive, not zero";
  }

  // Check that ldMaxWidth is valid.
  if( ldMaxWidth < 0 || ldMaxWidth > 8 )
  {
    std::ostringstream oss;
    oss << f << "Invalid ldMaxWidth (" << ldMaxWidth << 
      "); must be in the range [0,8]";
    throw oss.str();
  }

  // Get the bit-width of the number to be written, and subtract off one for
  // the leading '1' bit. (This also checks that the value is not negative.)
  int widthMinusOne = width( value ) - 1;

  // Check that the width of this is not greater than ldMaxWidth.
  if( width( widthMinusOne ) > ldMaxWidth )
  {
    std::ostringstream oss;
    oss << f << "ldMaxWidth = " << ldMaxWidth << " but needs to be " <<
      width( widthMinusOne ) << " for value 0x" << std::hex << value;
    throw oss.str();
  }

  // Write out one less than the width.
  fixed( widthMinusOne, ldMaxWidth );
  
  // Now write out the value itself, excluding the leading '1' bit.
  fixed( value, widthMinusOne );

  // Return the number of bits required.
  return ldMaxWidth + widthMinusOne;
}


// Out::boolean():
//   Write a boolean flag.
template< typename PositionType >
void Out< PositionType >::boolean( bool b )
{
  fixed( b ? 1 : 0, 1 );
}


// Out::flush():
//   Flush out any waiting bits.
template< typename PositionType >
void Out< PositionType >::flush()
{
  // Call the private method.
  bitsByte( 0, 0, true );
}


// Out::position():
//   Accessor for the current bit position.
template< typename PositionType >
PositionType Out< PositionType >::position() const
{
  return position_;
}


// In::In():
//   Constructor.
template< typename PositionType >
In< PositionType >::In( std::istream& instream ) :
  instream_( instream ),
  position_( 0 ),
  numBitsWaiting_( 0 ),
  lastNumBits_( 0 ),
  waitingBits_( 0 ),
  lastReadBits_( 0 ),
  doneExtra_( false )
{
}


// In::bits():
//   Read a number of bits into a vector of bytes.
template< typename PositionType >
template< typename NumBitsType >
void In< PositionType >::bits( std::vector< unsigned char >& data,
    NumBitsType numBits )
{
  // Set up invalid mask during instantiation.
  static const NumBitsType invalidMask = mask< NumBitsType >( -1 );

  // Check for a valid number of bits.
  static const std::string f = "Costella::Bitstream::In::bits(): ";
  if( numBits & invalidMask )
  {
    throw f + "Invalid negative number of bits";
  }

  // Clear the vector.
  data.clear();

  // Walk along the vector of bytes, and run down the number of bits, until
  // there are no bits left to read. Loop until there are eight or fewer
  // bits left to write.
  unsigned char byte;
  for( ; numBits > 8; numBits -= 8 )
  {
    // Read the bits of this byte.
    bitsByte( byte, 8 );

    // Store them.
    data.push_back( byte );
  }

  // Now do the final eight or fewer bits.
  bitsByte( byte, static_cast< int >( numBits ) );

  // Store them.
  data.push_back( byte );
}


// In::skip():
//   Skip a specified number of bits.
template< typename PositionType >
template< typename NumBitsType >
void In< PositionType >::skip( NumBitsType numBits )
{
  // Set up invalid mask during instantiation.
  static const NumBitsType invalidMask = mask< NumBitsType >( -1 );

  // Check for a valid number of bits.
  static const std::string f = "Costella::Bitstream::In::skip(): ";
  if( numBits & invalidMask )
  {
    throw f + "Invalid negative number of bits";
  }

  // Run down the number of bits until there are no bits left to skip. Loop
  // until there are eight or fewer bits left to skip.
  unsigned char byte;
  for( ; numBits > 8; numBits -= 8 )
  {
    // Read the bits of this byte.
    bitsByte( byte, 8 );
  }

  // Now do the final eight or fewer bits.
  bitsByte( byte, static_cast< int >( numBits ) );
}


// In::fixed():
//   Read a non-negative integer with a fixed bit-width.
template< typename PositionType >
template< typename ValueType >
void In< PositionType >::fixed( ValueType& value, int width )
{
  // Check that the bit-width is valid.
  static const std::string f = "Costella::Bitstream::In::fixed(): ";
  if( width < 0 )
  {
    std::ostringstream oss;
    oss << f << "Negative bit-width (" << width << ") not allowed"; 
    throw oss.str(); 
  }
  else if( width > std::numeric_limits< ValueType >::digits )
  {
    std::ostringstream oss;
    oss << f << "Specified bit-width (" << width << 
      ") too large for value's type (maximum width " << std::numeric_limits<
      ValueType >::digits << ")";
    throw oss.str(); 
  }

  // Start with nothing.
  value = 0;

  // Start with any bits we have modulo 8. This allows us to unread the
  // maximum possible number of bits.
  int odd = width & 0x7;
  if( odd )
  {
    // Read the odd bits.
    unsigned char byte;
    bitsByte( byte, odd );

    // Move them into position.
    if( width > 8 )
    {
      value |= static_cast< ValueType >( byte ) << ( width - 8 );
    }
    else
    {
      value |= static_cast< ValueType >( byte >> ( 8 - width ) );
      return;
    }

    // Update width remaining.
    width -= odd;
  }

  // The rest is a multiple of 8 bits. Keep going until we have no more bits
  // to do.
  while( width )
  {
    // Subtract 8 from the width.
    width -= 8;
    
    // Read 8 bits.
    unsigned char byte;
    bitsByte( byte, 8 );

    // Move them into position and add them in.
    value |= static_cast< ValueType >( byte ) << width;
  }
}


// In::variable():
//   Read a positive integer with a variable bit-width. See above for the
//   argument ldMaxWidth. Returns the total number of bits required.
template< typename PositionType >
template< typename ValueType >
int In< PositionType >::variable( ValueType& value, int ldMaxWidth )  
{
  // Set up vector of leading bit values on instantiation.
  static const std::vector< ValueType > leadingBits = leadingBitVector<
    ValueType >();

  // Read one less than the width.
  int widthMinusOne;
  fixed( widthMinusOne, ldMaxWidth );

  // Check that this is valid.
  if( widthMinusOne >= std::numeric_limits< ValueType >::digits )
  {
    // It's not valid. Unread these bits.
    unread( ldMaxWidth );

    // Throw an exception.
    static const std::string f = "Costella::Bitstream::In::variable(): ";
    std::ostringstream oss;
    oss << f << "Read bit-width - 1 = " << widthMinusOne << 
      "; too wide for specified value type (width = " <<
      std::numeric_limits< ValueType >::digits << ")";
    throw oss.str();
  }

  // Read the value itself, except for its leading '1' bit.
  fixed( value, widthMinusOne );

  // Add in the leading '1' bit.
  value |= leadingBits[ widthMinusOne ];

  // Return the number of bits required.
  return ldMaxWidth + widthMinusOne;
}


// In::boolean():
//   Read a Boolean flag.
template< typename PositionType >
void In< PositionType >::boolean( bool& b )
{
  fixed( b, 1 );
}


// In::unread():
//   Unread up to 8 bits.
template< typename PositionType >
void In< PositionType >::unread( int numBits )
{
  unsigned char byte;
  bitsByte( byte, numBits, true );
}


// In::flush():
//   Flush the input.
template< typename PositionType >
void In< PositionType >::flush()
{
  unsigned char byte;
  bitsByte( byte, 0, false, true );
}


// In::position():
//   Accessor for the current bit position.
template< typename PositionType >
PositionType In< PositionType >::position() const
{
  return position_;
}


// Out::bitsByte():
//   Private method for writing a number of bits from a single byte, sent in
//   from the most significant bit to the least significant bit; or for
//   flushing the object.
template< typename PositionType >
void Out< PositionType >::bitsByte( unsigned char byte, int numBits, bool
    flush )
{
  // Check for flush command.
  if( flush )
  {
    // Being told to flush the output. Check if we have any bits waiting.
    if( numBitsWaiting_ )
    {
      // We do. Update position to include padding bits. The waiting bits 
      // themselves have already been counted when they were previously 
      // added.
      position_ += static_cast< PositionType >( 8 - numBitsWaiting_ );

      // Write out the waiting bits.
      outstream_.put( static_cast< char >( waitingBits_ ) );

      // Clear the number of bits waiting.
      numBitsWaiting_ = 0;

      // Clear the byte of waiting bits.
      waitingBits_ = 0;
    }
  }
  else if( !numBits )
  {
    // Nothing to do. 
    return;
  }
  else
  {
    // Normal writing of a nonzero number of bits. Check that the position
    // is not going to overflow.
    if( std::numeric_limits< PositionType >::max() - static_cast<
        PositionType >( numBits ) < position_ )
    {
      static const std::string f = "Costella::Bitstream::Out::bitsByte(): ";
      std::ostringstream oss;
      oss << f << "Position type overflow: writing " << numBits << 
        " bits; previous position = " << position_;
      throw oss.str();
    }
    
    // Update position.
    position_ += static_cast< PositionType >( numBits );

    // Array of masks for keeping only the appropriate number of high bits. 
    static unsigned char mask[ 9 ] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8,
      0xfc, 0xfe, 0xff };

    // Mask off any irrelevant bits from the input byte.
    byte &= mask[ numBits ];

    // Add bits to the byte of bits already waiting.
    waitingBits_ |= byte >> numBitsWaiting_;

    // Compute the total number of bits we now have.
    int numBitsTotal = numBitsWaiting_ + numBits;

    // Check if we have enough to write out a complete byte, i.e., if the
    // number of bits in total is greater than or equal to eight (signified
    // by the high bit of its low nybble being set).
    if( numBitsTotal & 0x08 )
    {
      // We have enough to write out. Write it out.
      outstream_.put( static_cast< char >( waitingBits_ ) );

      // Subtract eight from the number of bits waiting, i.e., strip off
      // this high bit.
      numBitsTotal &= 0x07;

      // Construct the new byte of bits waiting, from those bits of 'byte'
      // that didn't get used above.
      waitingBits_ = byte << ( 8 - numBitsWaiting_ );
    }

    // Store the new number of bits waiting.
    numBitsWaiting_ = numBitsTotal;
  }
}


// In::bitsByte():
//   Private method for reading a number of bits to a single byte, stored
//   from the most significant bit to the least significant bit; or for
//   unreading up to eight bits; or for flushing the object to a byte
//   boundary.
template< typename PositionType >
void In< PositionType >::bitsByte( unsigned char& byte, int numBits, bool
    unread, bool flush )
{
  static const unsigned short mask[ 9 ] = { 0x0000, 0x8000, 0xc000, 0xe000,
    0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00 };
  static const std::string f = "Costella::Bitstream::In::bitsByte(): ";

  // Check for flush command.
  if( flush )
  {
    // Being told to flush the input. Check if we have any bits waiting.
    if( numBitsWaiting_ )
    {
      // We do. Compute the number of bits to flush, given that unreading
      // may have left us with eight or more bits waiting; we only flush to
      // the nearest byte boundary.
      int numBitsToFlush = numBitsWaiting_ & 0x7;
      
      // Update position to take into account the bits waiting we are
      // discarding.
      position_ += static_cast< PositionType >( numBitsToFlush );

      // Adjust the number of bits waiting.
      numBitsWaiting_ -= numBitsToFlush;

      // Discard bits where appropriate.
      waitingBits_ &= mask[ numBitsWaiting_ ];

      // Prevent an unread.
      lastNumBits_ = 0;
      lastReadBits_ = 0;
    }
  }
  else if( !numBits )
  {
    // Read or unread zero bits. Nothing to do except to prevent a further
    // unread. 
    lastNumBits_ = 0;
    lastReadBits_ = 0;
    byte = 0;
  }
  else if( unread )
  {
    // Unreading bits. Check if we are trying to unread too many.
    if( numBits > lastNumBits_ )
    {
      std::ostringstream oss;
      oss << f << "Trying to unread " << numBits << 
        " bits, but last read was only " << lastNumBits_ << " bits";
      throw oss.str();
    }

    // Check that the number of bits is not negative.
    if( numBits < 0 )
    {
      std::ostringstream oss;
      oss << f << "Trying to unread a negative number of bits (" << numBits
        << ")";
      throw oss.str();
    }

    // Subtract the specified number of bits from the position.
    position_ -= static_cast< PositionType >( numBits );

    // Get the bits we want to put back from lastReadBits_. Shift them left
    // by eight bits to put them into the second-lowest byte of an unsigned
    // short, and shift them left further by the number of bits that we
    // don't want to unread. Mask off to sixteen bits in case unsigned short
    // is not 16 bits.
    unsigned short restoreBits = ( lastReadBits_ << ( 8 + lastNumBits_ -
          numBits ) ) & 0xffff;

    // Shift the waiting bits right by numBits, to make room for the
    // restored bits.
    waitingBits_ >>= numBits;

    // Add the restore bits to the waiting bits.
    waitingBits_ |= restoreBits;

    // Increase the number of bits waiting by the number that we have just 
    // put back.
    numBitsWaiting_ += numBits;

    // Don't allow further unreading.
    lastNumBits_ = 0;
    lastReadBits_ = 0;
  }
  else
  {
    // Normal reading of bits. Check that the position is not going to
    // overflow.
    if( std::numeric_limits< PositionType >::max() - static_cast<
        PositionType >( numBits ) < position_ )
    {
      std::ostringstream oss;
      oss << f << "Position type overflow: reading " << numBits << 
        " bits; previous position = " << position_;
      throw oss.str();
    }
    position_ += static_cast< PositionType >( numBits );

    // Check if we don't have enough bits waiting to supply this request.
    if( numBitsWaiting_ < numBits )
    {
      // We don't have enough bits to supply. Read another byte.
      char charRead;
      if( instream_.get( charRead ).eof() )
      {
        if( doneExtra_ )
        {
          throw f + "End of input stream, including extra byte";
        }
        else
        {
          charRead = 0;
          doneExtra_ = true;
        }
      }
      unsigned char read = static_cast< unsigned char >( charRead );

      // Add the new byte to our supply.
      waitingBits_ |= ( static_cast< unsigned short >( read ) << 8 >>
          numBitsWaiting_ );
      numBitsWaiting_ += 8;
    }

    // Now get the bits we want. Store them at the same time.
    lastReadBits_ = ( waitingBits_ & mask[ numBits ] ) >> 8;
    byte = lastReadBits_;

    // Subtract the number of bits we have taken from those waiting.
    numBitsWaiting_ -= numBits; 

    // Shift the remaining waiting bits left.
    waitingBits_ <<= numBits;

    // Store the number of bits read.
    lastNumBits_ = numBits;
  }
}


// leadingBitVector():
//   Set up vector of leading bits for In::variable().
template< typename ValueType >
std::vector< ValueType > leadingBitVector()
{
  // Determine the size of the vector.
  int size = static_cast< int >( std::numeric_limits< ValueType >::digits );

  // Create the vector.
  std::vector< ValueType > leadingBits( size );

  // Initialize it.
  ValueType leadingBit = 1;
  for( int i = 0; i < size; i++, leadingBit <<= 1 )
  {
    leadingBits[ i ] = leadingBit;
  }

  // Return it.
  return leadingBits;
}


// width():
//   Returns the width of a non-negative integer. Note: This works for any
//   signed or unsigned integral type up to 256 bits in width. For integers
//   larger than this, extend in the obvious way.
template< typename ValueType >
int width( ValueType value )
{
  // Set up masks during instantiation.
  static const ValueType invalidMask = mask< ValueType >( -1 );
  static const ValueType mask0 = mask< ValueType >( 0 );
  static const ValueType mask1 = mask< ValueType >( 1 );
  static const ValueType mask2 = mask< ValueType >( 2 );
  static const ValueType mask3 = mask< ValueType >( 3 );
  static const ValueType mask4 = mask< ValueType >( 4 );
  static const ValueType mask5 = mask< ValueType >( 5 );
  static const ValueType mask6 = mask< ValueType >( 6 );
  static const ValueType mask7 = mask< ValueType >( 7 );
  
  // Check for invalid value.
  if( value & invalidMask )
  {
    static const std::string f = "Costella::Bitstream::width(): ";
    std::ostringstream oss;
    oss << f << "Invalid negative value (" << value << ")";
    throw oss.str();
  }
  
  // Deal with zero separately.
  if( !value )
  {
    return 0;
  }

  // Do the bisection search.
  int width = 1;
  if( value & mask7 )
  {
    width += 128;
    value &= mask7;
  }
  if( value & mask6 )
  {
    width += 64;
    value &= mask6;
  }
  if( value & mask5 )
  {
    width += 32;
    value &= mask5;
  }
  if( value & mask4 )
  {
    width += 16;
    value &= mask4;
  }
  if( value & mask3 )
  {
    width += 8;
    value &= mask3;
  }
  if( value & mask2 )
  {
    width += 4;
    value &= mask2;
  }
  if( value & mask1 )
  {
    width += 2;
    value &= mask1;
  }
  if( value & mask0 )
  {
    width++;
  }
  return width;  
}


// mask():
//   Used by width(). Creates one of the masks for the bisection search. If
//   the value of n is negative, return the invalid mask, which strips off
//   the sign bit for signed integers.
template< typename ValueType >
ValueType mask( int n )
{
  // Check that the type is integral.
  if( !std::numeric_limits< ValueType >::is_integer )
  {
    static const std::string f = "Costella::Bitstream::mask(): ";
    throw f + "Type is not integral!";
  }

  // Extract the number of (positive value) bits in this type.
  ValueType digits = static_cast< ValueType >( std::numeric_limits<
      ValueType >::digits );

  // Check for the invalid mask.
  if( n < 0 )
  {
    // Invalid mask. Only needed for signed types.
    if( std::numeric_limits< ValueType >::is_signed )
    {
      // Signed type. Mask out the sign bit.
      return static_cast< ValueType >( 1 ) << digits;
    }
    else
    {
      // Unsigned type. Return 0, which will not flag any values.
      return 0;
    }
  }
  else
  {
    // Normal mask.
    ValueType mask = 0;
    for( ValueType bit = 0; bit < digits; bit++ )
    {
      mask |= ( ( bit >> n ) & 0x1 ) << bit;
    }
    return mask;
  }
}


// variableBits():
//   Compute the number of bits required to encode a variable-width positive
//   integer.
template< typename ValueType >
int variableBits( ValueType value, int ldMaxWidth )
{
  // Check that value is not zero.
  static const std::string f = "Costella::Bitstream::variableBits(): ";
  if( !value )
  {
    throw f + "Value must be positive, not zero";
  }

  // Get the bit-width of the number to be written, and subtract off one for
  // the leading '1' bit. (This also checks that the value is not negative.)
  int widthMinusOne = width( value ) - 1;

  // Check that the width of this is not greater than ldMaxWidth.
  if( width( widthMinusOne ) > ldMaxWidth )
  {
    std::ostringstream oss;
    oss << f << "ldMaxWidth = " << ldMaxWidth << " but needs to be " <<
      width( widthMinusOne ) << " for value 0x" << std::hex << value;
    throw oss.str();
  }

  // Return the number of bits required.
  return ldMaxWidth + widthMinusOne;
}


// End the Costella::Bitstream namespace.
}}


// End of file.
