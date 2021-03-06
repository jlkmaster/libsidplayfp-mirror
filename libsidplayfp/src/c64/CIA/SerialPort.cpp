/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2017 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2009-2014 VICE Project
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2000 Simon White
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "interrupt.h"

#include "mos6526.h"

namespace libsidplayfp
{

void SerialPort::reset()
{
    out = 0;
    count = 0;
    buffered = false;
}

void SerialPort::event()
{
    parent.spInterrupt();
}

void SerialPort::handle(uint8_t serialDataReg)
{
    if (count && --count == 0)
    {
        eventScheduler.schedule(*this, 1, EVENT_CLOCK_PHI1);
    }

    if (count == 0 && buffered)
    {
        out = serialDataReg;
        buffered = false;
        count = 14;
        // Output rate 8 bits at ta / 2
    }
}

}
