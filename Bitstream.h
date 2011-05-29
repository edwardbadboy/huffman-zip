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
// Costella/Bitstream.h: 
//   Costella::Bitstream interface.
//
//   Class begun: February 17, 2006.
//   This version: April 29, 2008.
//
//   Written in ISO C++98.


// Include guard.
#ifndef COSTELLA_BITSTREAM_H
#define COSTELLA_BITSTREAM_H


// Include files.
#include <iostream>
#include <vector>


// Put inside the Costella::Bitstream namespace.
namespace Costella{ namespace Bitstream{


// Out:
//   Class for writing out a bitstream to a standard ostream.
//   PositionType is the type that should be used for counting the bit
//   position within the bitstream.
template< typename PositionType = int >
class Out
{
  public:

    // Constructor. 
    Out( std::ostream& outstream = std::cout );

    // Destructor.
    ~Out();

    // Write an arbitrary number of bits from a vector of bytes.
    template< typename NumBitsType >
    void bits( const std::vector< unsigned char >& data, NumBitsType numBits
        );

    // Write a non-negative integer with a fixed bit-width.
    template< typename ValueType >
    void fixed( ValueType value, int width );

    // Write a positive integer with a variable bit-width. The argument
    // ldMaxWidth (log to base 2 of the maximum bit-width) should be 
    //   ceil( ld( maxWidth ) )
    // i.e., 
    //   0 if the value must be 0x1 (1 bit)
    //   1 if the value is in [ 0x1, 0x3 ] (2 bits)
    //   2 for [ 0x1, 0xf ] (4 bits)
    //   3 for [ 0x1, 0xff ] (8 bits)
    //   4 for [ 0x1, 0xffff ] (16 bits)
    //   5 for [ 0x1, 0xffff:ffff ] (32 bits)
    //   6 for [ 0x1, 0xffff:ffff:ffff:ffff ] (64 bits)
    //   7 for [ 0x1, 0xffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ] (128 bits) 
    // The function returns the total number of bits required.
    template< typename ValueType >
    int variable( ValueType value, int ldMaxWidth );  

    // Output a Boolean flag.
    void boolean( bool b );

    // Flush out any waiting bits.
    void flush();

    // Accessor for the current bit position.
    PositionType position() const;

    // Default object. Will write to cout.
    static Out null;

  private:

    // Members.
    std::ostream& outstream_;
    PositionType position_;
    int numBitsWaiting_;
    unsigned char waitingBits_;

    // Methods.
    void bitsByte( unsigned char byte, int num_bits = 8, bool flush = false
        );
};


// In:
//   Class for reading in a bitstream from a standard istream.
//   PositionType is the type that should be used for counting the bit
//   position within the bitstream.
template< typename PositionType = int >
class In
{
  public:
    
    // Constructor.
    In( std::istream& instream = std::cin );

    // Read an arbitrary number of bits into a vector of bytes.
    template< typename NumBitsType >
    void bits( std::vector< unsigned char >& data, NumBitsType numBits );

    // Skip an arbitrary number of bits.
    template< typename NumBitsType >
    void skip( NumBitsType numBits );

    // Read a non-negative integer with a fixed bit-width.
    template< typename ValueType >
    void fixed( ValueType& value, int width );

    // Read a positive integer with a variable bit-width. (See above for the
    // argument ldMaxWidth.) Returns the total number of bits required.
    template< typename ValueType >
    int variable( ValueType& value, int ldMaxWidth );  

    // Read a Boolean flag.
    void boolean( bool& b );

    // Unread up to eight bits.
    void unread( int numBits );

    // Flush to a byte boundary.
    void flush();

    // Accessor for the current bit position.
    PositionType position() const;

    // Default object. Will read from cin.
    static In null;

  private:

    // Members.
    std::istream& instream_;
    PositionType position_;
    int numBitsWaiting_;
    int lastNumBits_;
    unsigned short waitingBits_;
    unsigned char lastReadBits_;
    bool doneExtra_;
    
    // Methods.
    void bitsByte( unsigned char& byte, int num_bits = 8, bool unread =
        false, bool flush = false );
};


// width():
//   Returns the bit-width of a non-negative integer.
template< typename ValueType >
int width( ValueType value );


// variableBits():
//   Compute the number of bits required to encode a variable-width positive
//   integer.
template< typename ValueType >
int variableBits( ValueType value, int ldMaxWidth );


// Close the Costella::Bitstream namespace.
}}


// End include guard.
#endif


// End of file.
