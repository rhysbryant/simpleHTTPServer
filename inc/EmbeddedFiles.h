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
#pragma once
#include <map>
#include <string>
#include "Request.h"
#include "Response.h"

using std::map;
using std::string;
using SimpleHTTP::Request;
using SimpleHTTP::Response;

namespace SimpleHTTP
{
    struct EmbeddedFile
    {
        const char *content;
        const int length;
        const char *fileName;
        /*
        first 6 bits typeType index
        next bit is reserved
        last is is is gzipped flag
        */
        const char flags;
    };

    struct EmbeddedFileType{
        const char* name;
        const int size;
    };

    class EmbeddedFilesHandler
    {
    private:
        static map<string, EmbeddedFile *> fileMap;
        static EmbeddedFileType* fileTypes;

    public:
        static void embeddedFilesHandler(Request *req, Response *resp);
        /*
         * this is intended to be used with genFiles.h.py
        * 
        */
        static void addFiles(EmbeddedFile *files,int filesCount, EmbeddedFileType* types);
    };
};