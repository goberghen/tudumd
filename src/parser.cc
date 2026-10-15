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

#include "parser.h"

Parser::Parser(const char* path)
{
	file.imbue(locale(""));
	file.open(path);
	collect_text = false;
	deadline = false;
	scheduled = false;
	first_todo = true;
}

Parser::~Parser()
{
	file.close();
}

bool Parser::parse(ToDo& todo, Sched& sched)
{
	if (!file) return false;

	/* Auto-detect format: check whether the file starts with an XML header
	 * or root tag (<?xml, <!DOCTYPE, or <todo). This allows Parser to load
	 * /usr/share/tudu/welcome.xml or legacy .tudu.xml files while never
	 * confusing a Markdown file that happens to contain '<' with XML. */
	wstring first_line;
	bool is_xml = false;
	while (getline(file, first_line))
	{
		size_t p = 0;
		while (p < first_line.length() &&
		       (first_line[p] == L' ' || first_line[p] == L'\t' || first_line[p] == L'\r'))
		{
			++p;
		}
		if (p >= first_line.length())
			continue;

		wstring sub = first_line.substr(p);
		if (sub.compare(0, 5, L"<?xml") == 0 ||
		    sub.compare(0, 9, L"<!DOCTYPE") == 0 ||
		    sub.compare(0, 5, L"<todo") == 0)
		{
			is_xml = true;
		}
		break;
	}

	file.clear();
	file.seekg(0, ios::beg);

	if (is_xml)
		return parseXml(todo, sched);
	else
		return parseMarkdown(todo, sched);
}

bool Parser::parseDateToken(const wstring& spec, int& d, int& m, int& y, int& pos)
{
	d = 0; m = 0; y = 0; pos = 0;
	wstring date_part = spec;

	size_t hash_pos = spec.find(L'#');
	if (hash_pos != wstring::npos)
	{
		date_part = spec.substr(0, hash_pos);
		wstring pos_str = spec.substr(hash_pos + 1);
		if (!pos_str.empty())
			pos = (int)wcstol(pos_str.c_str(), NULL, 10);
	}

	/* Accept DD/MM/YYYY, DD.MM.YYYY, or DD-MM-YYYY */
	vector<int> nums;
	wstring cur;
	for (size_t i = 0; i <= date_part.length(); ++i)
	{
		wchar_t c = (i < date_part.length()) ? date_part[i] : L'/';
		if (c == L'/' || c == L'.' || c == L'-')
		{
			if (cur.empty()) return false;
			nums.push_back((int)wcstol(cur.c_str(), NULL, 10));
			cur.clear();
		}
		else if (c >= L'0' && c <= L'9')
		{
			cur += c;
		}
		else
		{
			return false;
		}
	}
	if (nums.size() != 3) return false;
	d = nums[0];
	m = nums[1];
	y = nums[2];
	return (d >= 1 && d <= 31 && m >= 1 && m <= 12 && y >= 1900);
}

static void appendTextWithBr(wstring& current_text, bool& has_text, const wstring& raw)
{
	/* Split raw string on <br>, <br/>, <br /> (case-insensitive) */
	size_t i = 0;
	wstring seg;
	vector<wstring> parts;

	while (i < raw.length())
	{
		if (raw[i] == L'<' && i + 3 < raw.length() &&
		    (raw[i + 1] == L'b' || raw[i + 1] == L'B') &&
		    (raw[i + 2] == L'r' || raw[i + 2] == L'R'))
		{
			size_t j = i + 3;
			while (j < raw.length() && raw[j] == L' ') ++j;
			if (j < raw.length() && raw[j] == L'/') ++j;
			while (j < raw.length() && raw[j] == L' ') ++j;
			if (j < raw.length() && raw[j] == L'>')
			{
				parts.push_back(seg);
				seg.clear();
				i = j + 1;
				continue;
			}
		}
		seg += raw[i];
		++i;
	}
	parts.push_back(seg);

	for (size_t idx = 0; idx < parts.size(); ++idx)
	{
		wstring p = parts[idx];
		while (!p.empty() && p[p.length() - 1] == L' ')
			p.erase(p.length() - 1);
		if (idx == parts.size() - 1 && p.empty() && parts.size() > 1)
			continue;
		current_text += p + L"\n";
		has_text = true;
	}
}

void Parser::parseTaskLine(const wstring& rest_in, bool is_done, iToDo& iterator, Sched& sched)
{
	iterator->done() = is_done;
	iterator->getCollapse() = false;

	/* Check if user wrote <br> directly on the task line */
	wstring rest = rest_in;
	wstring inline_desc;
	for (size_t i = 0; i + 3 < rest.length(); ++i)
	{
		if (rest[i] == L'<' &&
		    (rest[i + 1] == L'b' || rest[i + 1] == L'B') &&
		    (rest[i + 2] == L'r' || rest[i + 2] == L'R'))
		{
			size_t j = i + 3;
			while (j < rest.length() && rest[j] == L' ') ++j;
			if (j < rest.length() && rest[j] == L'/') ++j;
			while (j < rest.length() && rest[j] == L' ') ++j;
			if (j < rest.length() && rest[j] == L'>')
			{
				inline_desc = rest.substr(j + 1);
				rest = rest.substr(0, i);
				break;
			}
		}
	}

	/* Split into whitespace-separated tokens to extract metadata tags
	 * while preserving normal words in the title. */
	vector<wstring> tokens;
	wstring cur;
	for (size_t i = 0; i < rest.length(); ++i)
	{
		wchar_t c = rest[i];
		if (c == L' ' || c == L'\t')
		{
			if (!cur.empty())
			{
				tokens.push_back(cur);
				cur.clear();
			}
		}
		else
		{
			cur += c;
		}
	}
	if (!cur.empty())
		tokens.push_back(cur);

	wstring title;
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		const wstring& tok = tokens[i];

		/* Collapsed branch marker: [+] */
		if (tok == L"[+]")
		{
			iterator->getCollapse() = true;
			continue;
		}

		/* Priority marker: !1 .. !5 */
		if (tok.length() == 2 && tok[0] == L'!' && tok[1] >= L'1' && tok[1] <= L'5')
		{
			iterator->priority() = (int)(tok[1] - L'0');
			continue;
		}

		/* Category marker: #category */
		if (tok.length() > 1 && tok[0] == L'#')
		{
			iterator->addCategory(tok.substr(1));
			continue;
		}

		/* Deadline marker: @due(DD/MM/YYYY) */
		if (tok.length() > 6 && tok.compare(0, 5, L"@due(") == 0 && tok[tok.length() - 1] == L')')
		{
			int d, m, y, pos;
			if (parseDateToken(tok.substr(5, tok.length() - 6), d, m, y, pos))
			{
				iterator->deadline().day() = d;
				iterator->deadline().month() = m;
				iterator->deadline().year() = y;
				continue;
			}
		}

		/* Scheduled marker: @sched(DD/MM/YYYY) or @sched(DD/MM/YYYY#pos) */
		if (tok.length() > 8 && tok.compare(0, 7, L"@sched(") == 0 && tok[tok.length() - 1] == L')')
		{
			int d, m, y, pos;
			if (parseDateToken(tok.substr(7, tok.length() - 8), d, m, y, pos))
			{
				iterator->sched().day() = d;
				iterator->sched().month() = m;
				iterator->sched().year() = y;
				iterator->schedPosition() = pos;
				if (iterator->sched().valid())
					sched.add(&(*iterator));
				continue;
			}
		}

		/* Regular word of the title */
		if (!title.empty()) title += L' ';
		title += tok;
	}

	iterator->getTitle() = title;
	if (!inline_desc.empty())
	{
		wstring desc_text;
		bool dummy = false;
		appendTextWithBr(desc_text, dummy, inline_desc);
		if (dummy)
			iterator->getText() = desc_text;
	}
}

bool Parser::parseMarkdown(ToDo& todo, Sched& sched)
{
	iToDo iterator(todo);
	vector<int> indent_stack;
	bool has_current = false;
	wstring current_text;
	bool has_text = false;
	int line_num = 0;

	wstring line;
	while (getline(file, line))
	{
		++line_num;
		if (!line.empty() && line[line.length() - 1] == L'\r')
			line.erase(line.length() - 1);

		/* Count leading indentation (1 tab = 4 spaces) */
		int indent = 0;
		size_t pos = 0;
		while (pos < line.length() && (line[pos] == L' ' || line[pos] == L'\t'))
		{
			indent += (line[pos] == L'\t') ? 4 : 1;
			++pos;
		}

		/* Skip blank lines */
		if (pos >= line.length())
			continue;

		/* Check if this line is a Markdown task:
		 * Standard: "- [ ] ...", "- [x] ...", "* [ ] ...", "* [x] ..."
		 * Also accept common manual-editing typos:
		 * "- [] ...", "-[ ] ...", "-[x] ...", "-[] ...", or plain bullet "- Task" */
		bool is_task = false;
		bool is_done = false;
		size_t rest_pos = 0;

		if (line[pos] == L'-' || line[pos] == L'*')
		{
			size_t p = pos + 1;
			while (p < line.length() && (line[p] == L' ' || line[p] == L'\t'))
				++p;

			if (p < line.length() && line[p] == L'[')
			{
				if (p + 2 < line.length() &&
				    (line[p + 1] == L' ' || line[p + 1] == L'x' || line[p + 1] == L'X') &&
				    line[p + 2] == L']' &&
				    (p + 3 == line.length() || line[p + 3] == L' ' || line[p + 3] == L'\t'))
				{
					is_task = true;
					is_done = (line[p + 1] == L'x' || line[p + 1] == L'X');
					rest_pos = (p + 3 < line.length()) ? (p + 4) : (p + 3);
				}
				else if (p + 1 < line.length() && line[p + 1] == L']' &&
				         (p + 2 == line.length() || line[p + 2] == L' ' || line[p + 2] == L'\t'))
				{
					/* Forgive "- [] Task" */
					is_task = true;
					is_done = false;
					rest_pos = (p + 2 < line.length()) ? (p + 3) : (p + 2);
				}
			}

			/* Forgive plain bullet "- Task" when there was a space after '-' */
			if (!is_task && p > pos + 1 && p < line.length())
			{
				is_task = true;
				is_done = false;
				rest_pos = p;
			}
		}

		if (is_task)
		{
			/* Save accumulated description text into previous task */
			if (has_current && has_text)
			{
				iterator->getText() = current_text;
				current_text.clear();
				has_text = false;
			}

			if (!has_current)
			{
				iterator.addChild(new ToDo());
				indent_stack.push_back(indent);
				has_current = true;
			}
			else if (indent > indent_stack.back())
			{
				iterator.in();
				iterator.addChild(new ToDo());
				indent_stack.push_back(indent);
			}
			else
			{
				while (indent_stack.size() > 1 && indent < indent_stack.back())
				{
					indent_stack.pop_back();
					iterator.out();
				}
				iterator.addChild(new ToDo());
				indent_stack.back() = indent;
			}

			parseTaskLine(line.substr(rest_pos), is_done, iterator, sched);
			wstring init_txt = iterator->getText().getStr();
			if (!init_txt.empty() && init_txt != L"\n")
			{
				current_text = init_txt;
				has_text = true;
			}
		}
		else if (has_current)
		{
			/* Description line belonging to the current task */
			if (line[pos] == L'>')
			{
				size_t text_start = pos + 1;
				if (text_start < line.length() && line[text_start] == L' ')
					++text_start;
				appendTextWithBr(current_text, has_text, line.substr(text_start));
			}
			else if (indent > indent_stack.back())
			{
				/* Indented continuation line under the current task */
				appendTextWithBr(current_text, has_text, line.substr(pos));
			}
			else
			{
				/* Unrecognized root/sibling line that is neither a task nor a description */
				fprintf(stderr, "Err: Invalid Markdown syntax at line %d.\n", line_num);
				return false;
			}
		}
		else
		{
			/* Non-empty line before the first task */
			fprintf(stderr, "Err: Unexpected line before first task at line %d.\n", line_num);
			return false;
		}
	}

	if (has_current && has_text)
		iterator->getText() = current_text;

	while (indent_stack.size() > 1)
	{
		indent_stack.pop_back();
		iterator.out();
	}

	if (!todo.haveChild())
		iterator.addChild(new ToDo());

	return true;
}

bool Parser::parseXml(ToDo& todo, Sched& sched)
{
	wchar_t ch;
	bool tag = false, att = false;
	iToDo iterator(todo);

	str = L"";
	collect_text = false;
	deadline = false;
	scheduled = false;
	first_todo = true;
	while (file.get(ch))
	{
		switch (ch)
		{
			case L'<':
				if (tag) return false;
				tag = true;
				if (collect_text)
				{
					txt = str;
					str = L"";
				}
				else collect_text = true;
				break;
			case L'>':
				if (!tag) return false;
				tag = false;
				collect_text = false;
				if (!att) ptag(iterator, sched);
				else {
					patt(iterator);
					att = false;
				}
				str = L"";
				break;
			case L'\t':
			case L' ':
				if (tag)
				{
					if (!att)
					{
						ptag(iterator, sched);
						att = true;
						str = L"";
					} else if (str.length()) {
						patt(iterator);
						str = L"";
					}
				} else
					if (collect_text) str += ch;
				break;
			case L'&':
				if (collect_text) str += amp();
				break;
			default:
				if (collect_text) str += ch;
		}
	}

	if (!todo.haveChild())
		iterator.addChild(new ToDo());

	return true;
}

wchar_t Parser::amp()
{
	wchar_t ch;
	wstring s = L"";

	file.get(ch);
	while (L';' != ch)
	{
		s += ch;
		file.get(ch);
	}
	if (L"amp" == s) return L'&';
	if (L"lt" == s) return L'<';
	if (L"gt" == s) return L'>';
	return L' ';
}

void Parser::ptag(iToDo& iterator, Sched& sched)
{
	if (L"todo" == str)
	{
		if (first_todo) first_todo = false;
		else {
			iterator.addChild(new ToDo());
			iterator.in();
		}
	}
	if (L"/todo" == str) iterator.out();
	if (L"title" == str) collect_text = true;
	if (L"/title" == str) iterator->getTitle() = txt;
	if (L"deadline" == str) deadline = true;
	if (L"/deadline" == str) deadline = false;
	if (L"day" == str) collect_text = true;
	if (L"/day" == str)
	{
		char num[3];
		wcstombs(num, txt.c_str(), 3);
		int day = atoi(num);
		if (deadline) iterator->deadline().day() = day;
		else if (scheduled) iterator->sched().day() = day;
	}
	if (L"month" == str) collect_text = true;
	if (L"/month" == str)
	{
		char num[3];
		wcstombs(num, txt.c_str(), 3);
		int month = atoi(num);
		if (deadline) iterator->deadline().month() = month;
		else if (scheduled) iterator->sched().month() = month;
	}
	if (L"year" == str) collect_text = true;
	if (L"/year" == str)
	{
		char num[5];
		wcstombs(num, txt.c_str(), 5);
		int year = atoi(num);
		if (deadline) iterator->deadline().year() = year;
		else if (scheduled) iterator->sched().year() = year;
	}
	if (L"position" == str) collect_text = true;
	if (L"/position" == str)
	{
		char num[8];
		wcstombs(num, txt.c_str(), 8);
		iterator->schedPosition() = atoi(num);
	}
	if (L"priority" == str) collect_text = true;
	if (L"/priority" == str)
	{
		char num[2];
		wcstombs(num, txt.c_str(), 2);
		iterator->priority() = atoi(num);
	}
	if (L"category" == str) collect_text = true;
	if (L"/category" == str) iterator->addCategory(txt);
	if (L"text" == str) collect_text = true;
	if (L"/text" == str)
	{
		if (!txt.empty() && L'\n' == txt[0]) txt.erase(0, 1);
		iterator->getText() = txt;
	}
	if (L"scheduled" == str) scheduled = true;
	if (L"/scheduled" == str)
	{
		if (iterator->sched().valid())
			sched.add(&(*iterator));
		scheduled = false;
	}
}

void Parser::patt(iToDo& iterator)
{
	wstring name, data;
	size_t eq_pos = str.find(L"=");

	name = str.substr(0, eq_pos);
	eq_pos++;
	data = str.substr(eq_pos, str.length() - eq_pos);
	if (L"done" == name)
	{
		iterator->done() = (L"\"yes\"" == data);
	}
	else if (L"collapse" == name)
	{
		iterator->getCollapse() = (L"\"yes\"" == data);
	}
}

Writer::Writer(const char* pathToSave, ToDo& t): todo(t)
{
	strncpy(path, pathToSave, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	file.imbue(locale(""));
}

Writer::~Writer() {}

bool Writer::save()
{
	/* Save a single backup of the previous file contents to /tmp/tudu.md.bak
	 * so working directories never get cluttered with .bak files */
	ifstream src_check(path, ios::binary | ios::ate);
	if (src_check.is_open() && src_check.tellg() > 0)
	{
		src_check.seekg(0, ios::beg);
		ofstream dst_bak("/tmp/tudu.md.bak", ios::binary | ios::trunc);
		if (dst_bak.is_open())
		{
			dst_bak << src_check.rdbuf();
			dst_bak.close();
		}
	}
	if (src_check.is_open())
		src_check.close();

	file.open(path);
	if (file.fail()) return false;

	i = new iToDo(todo);
	while (--(*i));
	_save();
	delete i;
	file.close();

	return true;
}

static void putSpaces(wofstream& file, int depth)
{
	for (int j = 0; j < depth; ++j)
		file << L"    ";
}

void Writer::_save()
{
	for (; !((*i).end()); ++(*i))
	{
		int depth = (*i).depth();
		putSpaces(file, depth);

		if ((*i)->done())
			file << L"- [x]";
		else
			file << L"- [ ]";

		if (L"" != (*i)->getTitle())
			file << L" " << (*i)->getTitle();

		if ((*i)->priority())
			file << L" !" << (*i)->priority();

		if (!(*i)->getCategories().empty())
		{
			for (set<wstring>::iterator it = (*i)->getCategories().begin();
			     it != (*i)->getCategories().end(); ++it)
			{
				file << L" #" << *it;
			}
		}

		if ((*i)->deadline().valid())
		{
			Date& date = (*i)->deadline();
			char buf[32];
			snprintf(buf, sizeof(buf), " @due(%02d.%02d.%04d)",
			         date.day(), date.month(), date.year());
			file << buf;
		}

		if ((*i)->sched().valid())
		{
			Date& date = (*i)->sched();
			char buf[48];
			if ((*i)->schedPosition() > 1)
			{
				snprintf(buf, sizeof(buf), " @sched(%02d.%02d.%04d#%d)",
				         date.day(), date.month(), date.year(), (*i)->schedPosition());
			}
			else
			{
				snprintf(buf, sizeof(buf), " @sched(%02d.%02d.%04d)",
				         date.day(), date.month(), date.year());
			}
			file << buf;
		}

		if ((*i)->getCollapse())
			file << L" [+]";

		file << endl;

		/* Write multi-line description if non-empty.
		 * Do NOT put a blank line before `>`, otherwise Discount (Okular)
		 * wraps the task title in <p>...</p> and breaks the line after the checkbox.
		 * Put a blank line after `>` ONLY if this task also has child tasks,
		 * so Discount closes <blockquote> before the child <ul>. */
		wstring text_str = (*i)->getText().getStr();
		if (!text_str.empty() && text_str != L"\n")
		{
			vector<wstring> tlines;
			size_t start = 0;
			while (start < text_str.length())
			{
				size_t nl = text_str.find(L'\n', start);
				if (nl == wstring::npos) nl = text_str.length();
				wstring line = text_str.substr(start, nl - start);
				if (nl == text_str.length() - 1 && line.empty())
					break;
				tlines.push_back(line);
				start = nl + 1;
			}
			for (size_t idx = 0; idx < tlines.size(); ++idx)
			{
				putSpaces(file, depth + 1);
				if (tlines[idx].empty())
					file << L">" << endl;
				else if (idx + 1 < tlines.size())
					file << L"> " << tlines[idx] << L"  " << endl;
				else
					file << L"> " << tlines[idx] << endl;
			}
			if ((*i)->haveChild())
				file << endl;
		}

		(*i).in();
		_save();
		(*i).out();
	}
}
