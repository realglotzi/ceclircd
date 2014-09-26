/*
    ceclircd -- LIRC daemon that reads CEC events from libcec
                https://github.com/Pulse-Eight/libcec
				
    Copyright (c) 2014 Dirk E. Wagner

    based on:
    inputlircd -- zeroconf LIRC daemon that reads from /dev/input/event devices
    Copyright (c) 2006  Guus Sliepen <guus@sliepen.eu.org>
	
    libcec-daemon -- A Linux daemon for connecting libcec to uinput.
    Copyright (c) 2012-2013, Andrew Brampton
     https://github.com/bramp/libcec-daemon
	
    This program is free software; you can redistribute it and/or modify it
    under the terms of version 2 of the GNU General Public License as published
    by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <iostream>
#include <libcec/cectypes.h>

namespace HDMI
{
    class physical_address
    {
        public:

        physical_address(uint16_t address=0) : address(address) {};
        physical_address(int a, int b=0, int c=0, int d=0) :
            address((a & 15)<<12|(b & 15)<<8|(c & 15)<<4|(d & 15)) {};

        operator uint16_t() const { return address; };
        int operator [] (int x) const { return (address>>((3 & ~x)*4)) & 15; };

        void set(const int (&val)[4])
        {
            address = (val[0] & 15)<<12|(val[1] & 15)<<8|(val[2] & 15)<<4|(val[3] & 15);
        }

        protected:

        uint16_t address;
    };

    class address
    {
        public:

        address() : physical(0), logical(CEC::CECDEVICE_UNKNOWN), port(0) {};

        physical_address physical;
        CEC::cec_logical_address logical;
        uint8_t port;
    };

    std::ostream& operator<<(std::ostream &out, const HDMI::physical_address & address);
    std::istream& operator>>(std::istream &in, HDMI::physical_address & address);

    std::ostream& operator<<(std::ostream &out, const HDMI::address & address);
    std::istream& operator>>(std::istream &in, HDMI::address & address);
};


