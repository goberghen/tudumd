/**************************************************************************
 * Copyright (C) 2007-2020 Ruben Pollan Bella <meskio@sindominio.net>     *
 * Copyright (C) 2026 goberghen & Arena.ai Agent (tudumd Markdown fork)   *
 *                                                                        *
 *  This file is part of tudumd (a Markdown-native fork of TuDu).         *
 *                                                                        *
 *  tudumd is free software; you can redistribute it and/or modify        *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; version 3 of the License.               *
 **************************************************************************/

#ifndef PARSER_H
#define PARSER_H

#include "includes.h"
#include "data.h"
#include "scheduler.h"
#include "text.h"

class Parser
{
public:
	Parser(const char* path);
	~Parser();

	bool parse(ToDo& todo, Sched& sched);
private:
	wifstream file;
	wstring str;
	wstring txt;
	bool collect_text;
	bool deadline;
	bool scheduled;
	bool first_todo;

	bool parseXml(ToDo& todo, Sched& sched);
	bool parseMarkdown(ToDo& todo, Sched& sched);
	void parseTaskLine(const wstring& rest, bool is_done, iToDo& iterator, Sched& sched);
	bool parseDateToken(const wstring& spec, int& d, int& m, int& y, int& pos);

	void ptag(iToDo& iterator, Sched& sched);
	void patt(iToDo& iterator);
	wchar_t amp();
};

class Writer
{
public:
	Writer(const char* pathToSave, ToDo& t);
	~Writer();

	bool save(); /* return if was successful */
private:
	wofstream file;
	ToDo& todo;
	iToDo* i;
	char path[4096];

	void _save();
};

#endif
