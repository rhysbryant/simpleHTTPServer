/*
 *  Copyright (c) 2023 Rhys Bryant
 *  Author Rhys Bryant
 *
 *	This file is part of SimpleHTTP
 *
 *   SimpleHTTP is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   any later version.
 *
 *   SimpleHTTP is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with SimpleHTTP.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "utility.h"
#include "string.h"

int SimpleHTTP::Utility::toASCII(int value, char* buffer, int base,int size)
{
    char bufTmp[10]="";
    int index = sizeof(bufTmp);


    do {
        auto tmp = value % base;
        value /= base;
        index--;
        bufTmp[index] = ASCIILookup[tmp];

    } while (value != 0);
    
    int outSize = sizeof(bufTmp) - index;
    memcpy(buffer,bufTmp+index,outSize);

    return outSize;
}
const constexpr char SimpleHTTP::Utility::ASCIILookup[];
