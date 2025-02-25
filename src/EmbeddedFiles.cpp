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
#include "EmbeddedFiles.h"

using SimpleHTTP::EmbeddedFile;
using SimpleHTTP::EmbeddedFilesHandler;

void EmbeddedFilesHandler::embeddedFilesHandler(Request* req, Response* resp) {
	auto f = fileMap[req->path];
	if (f == 0)
	{
		resp->writeHeader(SimpleHTTP::Response::NotFound);
		resp->write("the path was not found");
		return;
	}


	if (f->flags & 128)
	{
		auto accepts = req->headers["ACCEPT-ENCODING"];
		if (!accepts.empty() && accepts.find("gzip") != string::npos)
		{
			resp->writeHeaderLine(SIMPLE_STR("Content-Encoding: gzip"));
		}
		else
		{
			resp->writeHeader(SimpleHTTP::Response::InternalServerError);
			resp->write("compression support required for this file");
			return;
		}
	}

	resp->writeHeaderLine("Content-Type", fileTypes[(f->flags & 31)].name);
	resp->addContentLengthHeader(f->length);
	resp->writeDirect(f->content, f->length);

}

void EmbeddedFilesHandler::addFiles(EmbeddedFile* files, int filesCount,EmbeddedFileType* types) {
	for (int i = 0; i < filesCount; i++)
	{
		fileMap[files[i].fileName] = &files[i];
	}
	fileTypes = types;
}

map<string, EmbeddedFile *> EmbeddedFilesHandler::fileMap;
SimpleHTTP::EmbeddedFileType* EmbeddedFilesHandler::fileTypes;
